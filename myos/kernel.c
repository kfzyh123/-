/* kernel.c - myos graphical kernel (freestanding, 32-bit, VGA mode 13h).
 *
 * Loaded by boot.asm at 0x10000. Provides:
 *   - real 320x200x256 graphics drawn pixel by pixel to 0xA0000
 *   - own GDT/IDT, remapped PIC, 100 Hz PIT, keyboard scancode decoding
 *   - 12-user account system (1 SUPER top-level + root + 10 users, roles:
 *     SUPER / ROOT / TRUSTED / USER)
 *   - windowed desktop with 3 live windows (System Monitor / Terminal / About)
 *   - cooperative multi-tasking simulation driven by the timer IRQ
 *   - automatic crash -> recovery, plus boot-time "press twice" -> recovery
 *
 * Build: see build.py.
 */

#include <stdint.h>

#define VGA_W 320
#define VGA_H 200
#define VGA_MEM ((volatile uint8_t *)0xA0000)

/* ---- tiny libc ---- */
static void *kmemset(void *d, int v, unsigned n) {
    uint8_t *p = d; while (n--) *p++ = (uint8_t)v; return d;
}
static void *kmemcpy(void *d, const void *s, unsigned n) {
    uint8_t *dp = d; const uint8_t *sp = s;
    while (n--) *dp++ = *sp++; return d;
}
static unsigned kstrlen(const char *s) { unsigned n=0; while(s[n])n++; return n; }
static int kstrcmp(const char *a, const char *b) {
    while(*a && *a==*b){a++;b++;} return (uint8_t)*a-(uint8_t)*b;
}
static char *kstrcpy(char *d, const char *s){ char*r=d; while((*d++=*s++)); return r; }
static int kstrncmp(const char *a,const char*b,unsigned n){
    while(n-- && *a && *a==*b){a++;b++;} return n==(unsigned)-1?0:(uint8_t)*a-(uint8_t)*b;
}

/* ---- port I/O ---- */
static inline void outb(uint16_t p, uint8_t v){ __asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p)); }
static inline uint8_t inb(uint16_t p){ uint8_t v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outw(uint16_t p, uint16_t v){ __asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p)); }
static inline void io_wait(void){ outb(0x80,0); }

/* ---- 8x8 bitmap font (public-domain font8x8_basic subset, ASCII 0x20..0x7F) ----
 * Indexed by (ch - 0x20). Each char = 8 rows, MSB = leftmost pixel.
 * Only glyphs for characters actually printed are authored precisely; unmapped
 * chars render as a filled block so nothing ever looks broken. */
static const uint8_t font8[96][8] = {
 /*20 ' '*/{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
 /*21 '!'*/{0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
 /*22 '"'*/{0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00},
 /*23 '#'*/{0x66,0x66,0xff,0x66,0xff,0x66,0x66,0x00},
 /*24 '$'*/{0x18,0x3e,0x60,0x3c,0x06,0x7c,0x18,0x00},
 /*25 '%'*/{0x62,0x66,0x0c,0x18,0x30,0x66,0x46,0x00},
 /*26 '&'*/{0x3c,0x66,0x3c,0x38,0x67,0x66,0x3f,0x00},
 /*27 '''*/{0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00},
 /*28 '('*/{0x0e,0x18,0x30,0x30,0x30,0x18,0x0e,0x00},
 /*29 ')'*/{0x70,0x18,0x0c,0x0c,0x0c,0x18,0x70,0x00},
 /*2a '*'*/{0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00},
 /*2b '+'*/{0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00},
 /*2c ','*/{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
 /*2d '-'*/{0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00},
 /*2e '.'*/{0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
 /*2f '/'*/{0x00,0x06,0x0c,0x18,0x30,0x60,0x00,0x00},
 /*30 '0'*/{0x3c,0x66,0x6e,0x76,0x66,0x66,0x3c,0x00},
 /*31 '1'*/{0x18,0x38,0x18,0x18,0x18,0x18,0x7e,0x00},
 /*32 '2'*/{0x3c,0x66,0x06,0x0c,0x30,0x60,0x7e,0x00},
 /*33 '3'*/{0x3c,0x66,0x06,0x1c,0x06,0x66,0x3c,0x00},
 /*34 '4'*/{0x06,0x0e,0x1e,0x66,0x7f,0x06,0x06,0x00},
 /*35 '5'*/{0x7e,0x60,0x7c,0x06,0x06,0x66,0x3c,0x00},
 /*36 '6'*/{0x3c,0x60,0x7c,0x66,0x66,0x66,0x3c,0x00},
 /*37 '7'*/{0x7e,0x66,0x0c,0x18,0x18,0x18,0x18,0x00},
 /*38 '8'*/{0x3c,0x66,0x66,0x3c,0x66,0x66,0x3c,0x00},
 /*39 '9'*/{0x3c,0x66,0x66,0x3e,0x06,0x06,0x3c,0x00},
 /*3a ':'*/{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
 /*3b ';'*/{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
 /*3c '<'*/{0x06,0x0c,0x18,0x30,0x18,0x0c,0x06,0x00},
 /*3d '='*/{0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00},
 /*3e '>'*/{0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0x00},
 /*3f '?'*/{0x3c,0x66,0x06,0x0c,0x18,0x00,0x18,0x00},
 /*40 '@'*/{0x3c,0x66,0x6e,0x6e,0x60,0x62,0x3c,0x00},
 /*41 'A'*/{0x18,0x3c,0x66,0x66,0x7e,0x66,0x66,0x00},
 /*42 'B'*/{0x7c,0x66,0x66,0x7c,0x66,0x66,0x7c,0x00},
 /*43 'C'*/{0x3c,0x66,0x60,0x60,0x60,0x66,0x3c,0x00},
 /*44 'D'*/{0x78,0x6c,0x66,0x66,0x66,0x6c,0x78,0x00},
 /*45 'E'*/{0x7e,0x60,0x60,0x78,0x60,0x60,0x7e,0x00},
 /*46 'F'*/{0x7e,0x60,0x60,0x78,0x60,0x60,0x60,0x00},
 /*47 'G'*/{0x3c,0x66,0x60,0x6e,0x66,0x66,0x3c,0x00},
 /*48 'H'*/{0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x00},
 /*49 'I'*/{0x7e,0x18,0x18,0x18,0x18,0x18,0x7e,0x00},
 /*4a 'J'*/{0x06,0x06,0x06,0x06,0x06,0x66,0x3c,0x00},
 /*4b 'K'*/{0x66,0x6c,0x78,0x70,0x78,0x6c,0x66,0x00},
 /*4c 'L'*/{0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0x00},
 /*4d 'M'*/{0x63,0x77,0x7f,0x7f,0x6b,0x63,0x63,0x00},
 /*4e 'N'*/{0x66,0x76,0x7e,0x7e,0x6e,0x66,0x66,0x00},
 /*4f 'O'*/{0x3c,0x66,0x66,0x66,0x66,0x66,0x3c,0x00},
 /*50 'P'*/{0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0x00},
 /*51 'Q'*/{0x3c,0x66,0x66,0x66,0x66,0x3c,0x0e,0x00},
 /*52 'R'*/{0x7c,0x66,0x66,0x7c,0x78,0x6c,0x66,0x00},
 /*53 'S'*/{0x3c,0x66,0x60,0x3c,0x06,0x66,0x3c,0x00},
 /*54 'T'*/{0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
 /*55 'U'*/{0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00},
 /*56 'V'*/{0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0x00},
 /*57 'W'*/{0x63,0x63,0x6b,0x7f,0x7f,0x77,0x63,0x00},
 /*58 'X'*/{0x66,0x66,0x3c,0x18,0x3c,0x66,0x66,0x00},
 /*59 'Y'*/{0x66,0x66,0x66,0x3c,0x18,0x18,0x18,0x00},
 /*5a 'Z'*/{0x7e,0x06,0x0c,0x18,0x30,0x60,0x7e,0x00},
 /*5b '['*/{0x3c,0x30,0x30,0x30,0x30,0x30,0x3c,0x00},
 /*5c '\'*/{0x00,0x60,0x30,0x18,0x0c,0x06,0x00,0x00},
 /*5d ']'*/{0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00},
 /*5e '^'*/{0x10,0x38,0x6c,0x00,0x00,0x00,0x00,0x00},
 /*5f '_'*/{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
 /*60 '`'*/{0x60,0x60,0x18,0x00,0x00,0x00,0x00,0x00},
 /*61 'a'*/{0x00,0x00,0x3c,0x06,0x3e,0x66,0x3e,0x00},
 /*62 'b'*/{0x60,0x60,0x7c,0x66,0x66,0x66,0x7c,0x00},
 /*63 'c'*/{0x00,0x00,0x3c,0x60,0x60,0x60,0x3c,0x00},
 /*64 'd'*/{0x06,0x06,0x3e,0x66,0x66,0x66,0x3e,0x00},
 /*65 'e'*/{0x00,0x00,0x3c,0x66,0x7e,0x60,0x3c,0x00},
 /*66 'f'*/{0x0c,0x18,0x3e,0x18,0x18,0x18,0x18,0x00},
 /*67 'g'*/{0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x7c},
 /*68 'h'*/{0x60,0x60,0x7c,0x66,0x66,0x66,0x66,0x00},
 /*69 'i'*/{0x18,0x00,0x38,0x18,0x18,0x18,0x3c,0x00},
 /*6a 'j'*/{0x06,0x00,0x06,0x06,0x06,0x06,0x66,0x3c},
 /*6b 'k'*/{0x60,0x60,0x6c,0x78,0x78,0x6c,0x66,0x00},
 /*6c 'l'*/{0x38,0x18,0x18,0x18,0x18,0x18,0x3c,0x00},
 /*6d 'm'*/{0x00,0x00,0x66,0x7f,0x7f,0x6b,0x63,0x00},
 /*6e 'n'*/{0x00,0x00,0x7c,0x66,0x66,0x66,0x66,0x00},
 /*6f 'o'*/{0x00,0x00,0x3c,0x66,0x66,0x66,0x3c,0x00},
 /*70 'p'*/{0x00,0x00,0x7c,0x66,0x66,0x7c,0x60,0x60},
 /*71 'q'*/{0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x06},
 /*72 'r'*/{0x00,0x00,0x7c,0x66,0x60,0x60,0x60,0x00},
 /*73 's'*/{0x00,0x00,0x3e,0x60,0x3c,0x06,0x7c,0x00},
 /*74 't'*/{0x18,0x18,0x7e,0x18,0x18,0x18,0x0c,0x00},
 /*75 'u'*/{0x00,0x00,0x66,0x66,0x66,0x66,0x3e,0x00},
 /*76 'v'*/{0x00,0x00,0x66,0x66,0x66,0x3c,0x18,0x00},
 /*77 'w'*/{0x00,0x00,0x63,0x6b,0x7f,0x7f,0x36,0x00},
 /*78 'x'*/{0x00,0x00,0x66,0x3c,0x18,0x3c,0x66,0x00},
 /*79 'y'*/{0x00,0x00,0x66,0x66,0x66,0x3e,0x06,0x7c},
 /*7a 'z'*/{0x00,0x00,0x7e,0x0c,0x18,0x30,0x7e,0x00},
 /*7b '{'*/{0x0e,0x18,0x18,0x70,0x18,0x18,0x0e,0x00},
 /*7c '|'*/{0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
 /*7d '}'*/{0x70,0x18,0x18,0x0e,0x18,0x18,0x70,0x00},
 /*7e '~'*/{0x00,0x00,0x06,0x0e,0x00,0x00,0x00,0x00},
};

/* ---- palette (BIOS default VGA indices we rely on) ---- */
enum { C_BG=0, C_BLUE=1, C_GREEN=2, C_CYAN=3, C_RED=4, C_MAG=5,
       C_BROWN=6, C_LGRAY=7, C_DGRAY=8, C_LBLUE=9, C_LGREEN=10,
       C_LCYAN=11, C_LRED=12, C_LMAG=13, C_YELLOW=14, C_WHITE=15 };

/* forward globals (used by ISRs defined above the windows section) */
static volatile int g_ticks = 0;     /* scheduler tick counter (IRQ0) */
static int g_auth_user = -1;         /* logged-in user index, -1 = none */

/* ---- drawing primitives ---- */
static void px(int x,int y,uint8_t c){ if((unsigned)x<VGA_W&&(unsigned)y<VGA_H) VGA_MEM[y*VGA_W+x]=c; }
static void fill_rect(int x,int y,int w,int h,uint8_t c){
    for(int j=0;j<h;j++)for(int i=0;i<w;i++) px(x+i,y+j,c);
}
static void fill_screen(uint8_t c){ kmemset((void*)VGA_MEM,c,VGA_W*VGA_H); }
static void draw_char(int x,int y,char ch,uint8_t fg,uint8_t bg){
    if(ch<0x20||ch>0x7e){ fill_rect(x,y,8,8,bg); return; }
    const uint8_t *g = font8[(uint8_t)ch-0x20];
    for(int r=0;r<8;r++){ uint8_t bits=g[r]; for(int c=0;c<8;c++)
        px(x+c,y+r,(bits&(0x80>>c))?fg:bg); }
}
static void draw_text(int x,int y,const char*s,uint8_t fg,uint8_t bg){
    while(*s){ draw_char(x,y,*s,fg,bg); x+=8; s++; }
}
/* draw text that may extend; no wrapping */
static void draw_text_n(int x,int y,const char*s,int n,uint8_t fg,uint8_t bg){
    for(int i=0;i<n&&s[i];i++){ draw_char(x,y,s[i],fg,bg); x+=8; }
}
static void hline(int x,int y,int w,uint8_t c){ for(int i=0;i<w;i++) px(x+i,y,c); }
static void vline(int x,int y,int h,uint8_t c){ for(int i=0;i<h;i++) px(x,y+i,c); }
static void rect_outline(int x,int y,int w,int h,uint8_t c){
    hline(x,y,w,c); hline(x,y+h-1,w,c); vline(x,y,h,c); vline(x+w-1,y,h,c);
}

/* ---- GDT / IDT ---- */
struct gdt_entry { uint16_t lim_low, base_low; uint8_t base_mid, access, gran; };
struct gdt_ptr  { uint16_t lim; uint32_t base; };
struct idt_entry { uint16_t base_low; uint16_t sel; uint8_t zero; uint8_t flags; uint16_t base_hi; };
struct idt_ptr  { uint16_t lim; uint32_t base; };

static struct gdt_entry gdt[3] __attribute__((aligned(8)));
static struct gdt_ptr   gdtp;
static struct idt_entry idt[256] __attribute__((aligned(8)));
static struct idt_ptr   idtp;

extern void load_gdt(struct gdt_ptr*);
extern void load_idt(struct idt_ptr*);
extern void isr_default(void);
extern void irq_timer(void);
extern void irq_keyboard(void);

static void gdt_set(int i,uint32_t base,uint32_t lim,uint8_t acc,uint8_t gran){
    gdt[i].base_low=base&0xffff; gdt[i].base_mid=(base>>16)&0xff;
    gdt[i].lim_low=lim&0xffff; gdt[i].gran=(gran&0xf0)|((lim>>16)&0x0f);
    gdt[i].access=acc;
}
static void idt_set(int i,uint32_t base,uint16_t sel,uint8_t flags){
    idt[i].base_low=base&0xffff; idt[i].base_hi=(base>>16)&0xffff;
    idt[i].sel=sel; idt[i].zero=0; idt[i].flags=flags;
}

static void pic_remap(void){
    /* ICW1: init both, ICW4 needed */
    outb(0x20,0x11); io_wait(); outb(0xA0,0x11); io_wait();
    /* ICW2: vector offsets master 0x20, slave 0x28 */
    outb(0x21,0x20); io_wait(); outb(0xA1,0x28); io_wait();
    /* ICW3: master cascade to slave on IRQ2, slave cascade identity 2 */
    outb(0x21,0x04); io_wait(); outb(0xA1,0x02); io_wait();
    /* ICW4: 8086 mode, auto EOI off */
    outb(0x21,0x01); io_wait(); outb(0xA1,0x01); io_wait();
    /* mask: enable IRQ0 (timer) + IRQ1 (keyboard), mask the rest */
    outb(0x21,0xFC); outb(0xA1,0xFF);
}

static void pit_init(void){
    uint16_t d = 11932;           /* 1193182 / 100 Hz ~= 11932 */
    outb(0x43,0x36);              /* channel 0, mode 3 (square), binary */
    outb(0x40,d&0xff);
    outb(0x40,(d>>8)&0xff);
}

static void gdt_idt_init(void){
    gdt_set(0,0,0,0,0);
    gdt_set(1,0,0xfffff,0x9A,0xCF);    /* code  ring0 */
    gdt_set(2,0,0xfffff,0x92,0xCF);    /* data  ring0 */
    gdtp.lim=sizeof(gdt)-1; gdtp.base=(uint32_t)gdt;
    load_gdt(&gdtp);
    for(int i=0;i<256;i++) idt_set(i,(uint32_t)isr_default,0x08,0x8E);
    idt_set(0x20,(uint32_t)irq_timer,0x08,0x8E);
    idt_set(0x21,(uint32_t)irq_keyboard,0x08,0x8E);
    idtp.lim=sizeof(idt)-1; idtp.base=(uint32_t)idt;
    load_idt(&idtp);
    pic_remap();
    pit_init();
}

/* ---- keyboard (PS/2 scancode set 1) ---- */
#define KEYBUF 64
static volatile uint8_t  kbuf[KEYBUF];
static volatile unsigned kbuf_r=0, kbuf_w=0;
static volatile uint8_t  shift_down=0;

/* base scancode -> ascii (lowercase letters + digits + common punct) */
static char sc_base(uint8_t s){
    switch(s){
    case 0x01: return 0x1b;                 /* Esc */
    case 0x0f: return '\t';                /* Tab */
    case 0x1c: return '\n';
    case 0x0e: return '\b';
    case 0x39: return ' ';
    case 0x02: return '1'; case 0x03: return '2'; case 0x04: return '3';
    case 0x05: return '4'; case 0x06: return '5'; case 0x07: return '6';
    case 0x08: return '7'; case 0x09: return '8'; case 0x0a: return '9';
    case 0x0b: return '0';
    case 0x0c: return '-'; case 0x0d: return '='; case 0x1a: return '[';
    case 0x1b: return ']'; case 0x27: return ';'; case 0x28: return '\'';
    case 0x29: return '`'; case 0x2b: return '\\'; case 0x33: return ',';
    case 0x34: return '.'; case 0x35: return '/';
    case 0x10: return 'q'; case 0x11: return 'w'; case 0x12: return 'e';
    case 0x13: return 'r'; case 0x14: return 't'; case 0x15: return 'y';
    case 0x16: return 'u'; case 0x17: return 'i'; case 0x18: return 'o';
    case 0x19: return 'p'; case 0x1e: return 'a'; case 0x1f: return 's';
    case 0x20: return 'd'; case 0x21: return 'f'; case 0x22: return 'g';
    case 0x23: return 'h'; case 0x24: return 'j'; case 0x25: return 'k';
    case 0x26: return 'l'; case 0x2c: return 'z'; case 0x2d: return 'x';
    case 0x2e: return 'c'; case 0x2f: return 'v'; case 0x30: return 'b';
    case 0x31: return 'n'; case 0x32: return 'm';
    }
    return 0;
}
/* shift applied to a base ascii */
static char sc_shift(char c){
    if(c>='a'&&c<='z') return c-32;
    static const char *d="1234567890-=[];'#,./";
    static const char *s="!@#$%^&*()_+{}:\"|<>?";
    for(int i=0;d[i];i++) if(c==d[i]) return s[i];
    if(c=='`') return '~'; if(c=='\\') return '|';
    return c;
}

/* ISR handlers (called from entry.asm stubs) */
void irq_keyboard_handler(void){
    uint8_t s = inb(0x60);
    if(s==0x2a||s==0x36) shift_down=1;
    else if(s==0xaa||s==0xb6) shift_down=0;
    else if(!(s&0x80)){                 /* key press */
        char c = sc_base(s&0x7f);
        if(c){
            if(shift_down) c=sc_shift(c);
            kbuf[kbuf_w]=c; kbuf_w=(kbuf_w+1)%KEYBUF;
        }
    }
    /* discard release */
}
void irq_timer_handler(void){ g_ticks++; }

/* blocking key read */
static char get_char(void){
    while(kbuf_r==kbuf_w){ __asm__ volatile("hlt"); }
    char c=kbuf[kbuf_r]; kbuf_r=(kbuf_r+1)%KEYBUF; return c;
}
/* read a line; masked -> echo '*', supports backspace/enter */
static void read_line(char*buf,int max,int masked){
    int n=0; char c;
    while((c=get_char())!='\n'){
        if(c=='\b'){ if(n>0) n--; }
        else if(n<max-1){ buf[n++]=c; }
    }
    buf[n]=0;
}

/* ---- users ---- */
enum { ROLE_SUPER=0, ROLE_ROOT=1, ROLE_TRUSTED=2, ROLE_USER=3 };
struct user { const char*name; const char*pass; int role; int hidden; };
/* 12 users: SUPER (hidden, kernel-only) + root + 10 others.
 * SUPER is the "top-level" account not shown on normal login. */
static const struct user USERS[12]={
    {"super","root",      ROLE_SUPER,  1},
    {"root","toor",       ROLE_ROOT,   0},
    {"admin","admin1",    ROLE_TRUSTED,0},
    {"alice","alice1",    ROLE_TRUSTED,0},
    {"bob","bob123",      ROLE_USER,   0},
    {"carol","carol1",    ROLE_USER,   0},
    {"dave","dave11",     ROLE_TRUSTED,0},
    {"eve","eve123",      ROLE_USER,   0},
    {"frank","frank1",    ROLE_USER,   0},
    {"grace","grace1",    ROLE_TRUSTED,0},
    {"heidi","heidi1",    ROLE_USER,   0},
    {"ivan","ivan11",     ROLE_USER,   0},
};
static const char*role_name(int r){
    return r==ROLE_SUPER?"SUPER":r==ROLE_ROOT?"ROOT":r==ROLE_TRUSTED?"TRUSTED":"USER";
}

/* ---- windows (cooperative multi-task demo) ---- */
struct win { int x,y,w,h; const char*title; };
static const struct win WINS[3]={
    {8,16, 150,80,"System Monitor"},
    {8,100,150,80,"Terminal"},
    {165,16,147,164,"About"},
};
static int focus=0;            /* which window has focus */

static void draw_window(const struct win*w,int focused){
    uint8_t bcol = focused?C_LBLUE:C_DGRAY;
    uint8_t tcol = focused?C_WHITE:C_LGRAY;
    fill_rect(w->x,w->y,w->w,w->h,C_LGRAY);            /* body light-gray */
    fill_rect(w->x,w->y,w->w,8,bcol);                       /* title bar */
    draw_text(w->x+4,w->y,w->title,tcol,bcol);
    rect_outline(w->x,w->y,w->w,w->h,bcol);
}
static void win_put(const struct win*w,int row,int col,const char*s,uint8_t fg){
    draw_text(w->x+3+col*8, w->y+11+row*9, s, fg, C_LGRAY);
}

static void itoa(int v,char*buf){
    char t[12]; int i=0; unsigned u = v<0? -(unsigned)v : (unsigned)v;
    if(v<0)*buf++='-';
    if(u==0){*buf='0';buf[1]=0;return;}
    while(u){t[i++]='0'+u%10; u/=10;}
    while(i--)*buf++=t[i]; *buf=0;
}

/* ---- CPU info via CPUID ---- */
static void cpuid_vendor(char*buf){
    int a,b,c,d;
    __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0));
    int*p=(int*)buf; p[0]=b; p[1]=d; p[2]=c; buf[12]=0;
}
static int cpuid_has_apic(void){
    int a,b,c,d; __asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1));
    return (d>>9)&1;
}

/* ---- desktop ---- */
static void draw_desktop(void){
    fill_screen(C_DGRAY);                       /* wallpaper base */
    /* subtle gradient */
    for(int y=0;y<VGA_H;y++){
        uint8_t c = 8 + (y*7/VGA_H);
        hline(0,y,VGA_W,c);
    }
    /* top bar */
    fill_rect(0,0,VGA_W,12,C_BLUE);
    draw_text(4,2,"myos  -  graphical kernel",C_WHITE,C_BLUE);
    /* clock / uptime */
    char tb[40]; kstrcpy(tb,"T:"); itoa(g_ticks,tb+2); draw_text(VGA_W-80,2,tb,C_YELLOW,C_BLUE);
    /* status bar */
    fill_rect(0,VGA_H-10,VGA_W,10,C_DGRAY);
    const char*u = g_auth_user>=0?USERS[g_auth_user].name:"not logged in";
    char sb[64]; kstrcpy(sb,"user:"); kstrcpy(sb+5,u);
    draw_text(4,VGA_H-9,sb,C_LGRAY,C_DGRAY);
    draw_text(VGA_W-150,VGA_H-9,"F1/F2/F3 win  Tab cycle  Esc logout",C_DGRAY,C_DGRAY==7?8:7);
}
static void draw_windows(void){
    for(int i=0;i<3;i++) draw_window(&WINS[i], i==focus);
}

/* "processes" update their windows based on global tick - simulates multi-tasking */
static void update_sysmon(void){
    const struct win*w=&WINS[0];
    char b[40];
    draw_text(w->x+3,w->y+11,"SYSTEM MONITOR",C_WHITE,C_LGRAY);
    kstrcpy(b,"ticks: "); itoa(g_ticks,b+7);
    draw_text(w->x+3,w->y+22,b,C_GREEN,C_LGRAY);
    char cpu[20]; cpuid_vendor(cpu);
    kstrcpy(b,"cpu: "); kstrcpy(b+5,cpu);
    draw_text(w->x+3,w->y+33,b,C_CYAN,C_LGRAY);
    kstrcpy(b,"apic: "); itoa(cpuid_has_apic(),b+6);
    draw_text(w->x+3,w->y+44,b,C_CYAN,C_LGRAY);
    kstrcpy(b,"mem: 32M (assumed)");
    draw_text(w->x+3,w->y+55,b,C_LGRAY,C_LGRAY);
    kstrcpy(b,"users: 12 (1 hidden)");
    draw_text(w->x+3,w->y+66,b,C_YELLOW,C_LGRAY);
}
static void update_terminal(void){
    const struct win*w=&WINS[1];
    int phase = (g_ticks/10)%6;
    const char*lines[6]={
        "[sys] scheduler tick",
        "[sys] task switch ok",
        "[sys] vga frame drawn",
        "[net] no link (standalone)",
        "[sec] audit log clean",
        "[usr] session active",
    };
    for(int i=0;i<6;i++){
        int active = (phase==i);
        draw_text(w->x+3,w->y+11+i*10, lines[i],
                  active?C_WHITE:C_DGRAY, C_LGRAY);
    }
    draw_text(w->x+3,w->y+71,"myos$ _",C_GREEN,C_LGRAY);
}
static void update_about(void){
    const struct win*w=&WINS[2];
    draw_text(w->x+6,w->y+12,"myos",C_WHITE,C_LGRAY);
    draw_text(w->x+6,w->y+24,"self-made OS",C_LCYAN,C_LGRAY);
    draw_text(w->x+6,w->y+36,"no dependencies",C_LCYAN,C_LGRAY);
    draw_text(w->x+6,w->y+48,"boots from USB",C_LCYAN,C_LGRAY);
    draw_text(w->x+6,w->y+60,"12-user model",C_LCYAN,C_LGRAY);
    draw_text(w->x+6,w->y+72,"crash -> recovery",C_LCYAN,C_LGRAY);
    draw_text(w->x+6,w->y+84,"press 2x at boot",C_LCYAN,C_LGRAY);
    draw_text(w->x+6,w->y+96,"= recovery mode",C_LCYAN,C_LGRAY);
    draw_text(w->x+6,w->y+108,"FOSS / no copyright",C_YELLOW,C_LGRAY);
    draw_text(w->x+6,w->y+120,"infringement",C_YELLOW,C_LGRAY);
    draw_text(w->x+6,w->y+140,"(c) public domain",C_DGRAY,C_LGRAY);
}

/* ---- login screen ---- */
static int do_login(void){
    /* list non-hidden users */
    int shown=0; int map[12];
    fill_screen(C_DGRAY);
    fill_rect(0,0,VGA_W,40,C_BLUE);
    draw_text(60,8,"myos - login",C_WHITE,C_BLUE);
    draw_text(4,18,"select user (number), then enter password",C_LGRAY,C_DGRAY);
    int y=50;
    for(int i=0;i<12;i++){
        if(USERS[i].hidden) continue;
        map[shown]=i;
        char line[64]; char n[4];
        /* "1 root  [ROOT]" style */
        itoa(shown+1,n);
        kstrcpy(line,n); kstrcpy(line+kstrlen(line)," ");
        kstrcpy(line+kstrlen(line),USERS[i].name);
        kstrcpy(line+kstrlen(line),"  [");
        kstrcpy(line+kstrlen(line),role_name(USERS[i].role));
        kstrcpy(line+kstrlen(line),"]");
        draw_text(20,y,line,C_LGREEN,C_DGRAY);
        y+=10; shown++;
    }
    draw_text(4,150,"0 = recovery shell",C_YELLOW,C_DGRAY);
    draw_text(4,160,"pick: ",C_WHITE,C_DGRAY);
    /* read a digit */
    char c; int pick=-1;
    while(1){ c=get_char();
        if(c>='0'&&c<='9'){ pick=c-'0'; break; }
    }
    if(pick==0){ return -2; }                 /* recovery requested */
    if(pick<1||pick>shown){ return -1; }
    int ui = map[pick-1];
    /* password */
    fill_rect(4,160,200,8,C_DGRAY);
    draw_text(4,160,"passwd: ",C_WHITE,C_DGRAY);
    char pw[16]; read_line(pw,sizeof pw,1);
    if(kstrcmp(pw,USERS[ui].pass)==0) return ui;
    draw_text(4,170,"wrong password",C_LRED,C_DGRAY);
    for(volatile int z=0;z<4000000;z++);
    return -1;
}

/* ---- recovery environment ---- */
static int g_recovery_super=0;       /* SUPER authenticated in recovery? */
static void recovery_draw_prompt(int row,const char*buf){
    fill_rect(0,0,VGA_W,VGA_H,C_DGRAY==0?1:0);
    fill_rect(0,0,VGA_W,12,C_RED);
    draw_text(4,2,"myos RECOVERY ENVIRONMENT",C_WHITE,C_RED);
    draw_text(4,30,g_recovery_super?
        "[SUPER] kernel-level authority":"[guest] type 'login super' for full power",
        g_recovery_super?C_YELLOW:C_LGRAY,0);
    draw_text(4,20+row*10,"recovery> ",C_GREEN,0);
    draw_text(4+9*8,20+row*10,buf,C_WHITE,0);
}
static void recovery_print(int*row,const char*s,uint8_t col){
    draw_text(4,20+(*row)*10,s,col,0); (*row)++;
}
static int do_recovery(void){
    int row=1;
    recovery_draw_prompt(row,"");
    while(1){
        char buf[64]; buf[0]=0; int n=0;
        while(1){
            recovery_draw_prompt(row,buf);
            char c=get_char();
            if(c=='\n') break;
            if(c=='\b'){ if(n>0) buf[--n]=0; }
            else if(n<63) buf[n++]=c, buf[n]=0;
        }
        recovery_draw_prompt(row,buf);   /* commit the line */
        row++;
        /* parse */
        if(kstrcmp(buf,"help")==0){
            recovery_print(&row,"commands:",C_LCYAN);
            recovery_print(&row,"  help  users  cpu  mem  whoami",C_LGRAY);
            recovery_print(&row,"  login super  kernel  reboot  exit",C_LGRAY);
        } else if(kstrcmp(buf,"users")==0){
            for(int i=0;i<12;i++){
                char line[64]; kstrcpy(line,USERS[i].name);
                kstrcpy(line+kstrlen(line)," [");
                kstrcpy(line+kstrlen(line),role_name(USERS[i].role));
                kstrcpy(line+kstrlen(line),USERS[i].hidden?"] hidden":"]");
                recovery_print(&row,line,C_LGREEN);
            }
        } else if(kstrcmp(buf,"cpu")==0){
            char cpu[20]; cpuid_vendor(cpu);
            char line[40]="vendor: "; kstrcpy(line+8,cpu);
            recovery_print(&row,line,C_CYAN);
        } else if(kstrcmp(buf,"mem")==0){
            recovery_print(&row,"memory: 32 MiB (mem map)",C_CYAN);
        } else if(kstrcmp(buf,"whoami")==0){
            recovery_print(&row,g_recovery_super?"super":"guest",C_YELLOW);
        } else if(kstrcmp(buf,"login super")==0){
            recovery_print(&row,"super password:",C_LRED);
            char pw[16]; read_line(pw,sizeof pw,1);
            if(kstrcmp(pw,USERS[0].pass)==0){
                g_recovery_super=1;
                recovery_print(&row,"SUPER authenticated. full power.",C_YELLOW);
            } else {
                recovery_print(&row,"denied.",C_LRED);
            }
        } else if(kstrcmp(buf,"kernel")==0){
            if(g_recovery_super){
                recovery_print(&row,"[KERNEL] direct code execution:",C_YELLOW);
                recovery_print(&row,"  halt cpu / wipe state / remap hw",C_LGRAY);
                recovery_print(&row,"  -> this is the top-level authority",C_LGRAY);
            } else {
                recovery_print(&row,"denied: need SUPER",C_LRED);
            }
        } else if(kstrcmp(buf,"reboot")==0){
            outb(0x64,0xFE);            /* reset via keyboard controller */
            while(1) __asm__ volatile("hlt");
        } else if(kstrcmp(buf,"exit")==0){
            recovery_print(&row,"returning to normal boot...",C_LCYAN);
            for(volatile int z=0;z<3000000;z++);
            return 0;                   /* go back to normal boot */
        } else if(buf[0]==0){
            /* empty */
        } else {
            recovery_print(&row,"unknown command",C_LRED);
        }
        if(row>16){ row=1; fill_rect(0,12,VGA_W,VGA_H-12,0); }
    }
}

/* ---- crash handler (called from isr_default stub) ---- */
void isr_default_handler(void){
    /* never returns: auto-enter recovery. Re-enable IRQs so the recovery
     * shell can still read the keyboard / use the timer. */
    __asm__ volatile("sti");
    g_recovery_super=0;
    fill_screen(C_RED);
    draw_text(40,90,"*** SYSTEM FAULT ***",C_WHITE,C_RED);
    draw_text(40,104,"auto-recovery engaged",C_YELLOW,C_RED);
    for(volatile int z=0;z<8000000;z++);
    do_recovery();            /* loops forever or until exit->reboot path */
    while(1){ __asm__ volatile("cli"); __asm__ volatile("hlt"); }
}

/* ---- boot splash ---- */
static void splash(void){
    fill_screen(1);                       /* deep blue */
    for(int i=0;i<60;i++) hline(0,70+i,VGA_W,1+(i/9));
    draw_text(120,80,"myos",C_WHITE,1);
    draw_text(60,100,"self-made graphical OS",C_LCYAN,1);
    draw_text(50,116,"boots from USB - no host OS",C_LGRAY,1);
    draw_text(70,132,"public domain / FOSS",C_YELLOW,1);
    for(volatile int z=0;z<8000000;z++);
}

/* ---- desktop main loop ---- */
static void desktop(void){
    int last_redraw=-1;
    while(1){
        if(g_ticks!=last_redraw){
            last_redraw=g_ticks;
            draw_desktop();
            draw_windows();
            update_sysmon();
            update_terminal();
            update_about();
        }
        /* non-blocking key processing */
        if(kbuf_r!=kbuf_w){
            char c = kbuf[kbuf_r]; kbuf_r=(kbuf_r+1)%KEYBUF;
            if(c==0x1b){ g_auth_user=-1; return; }    /* Esc: logout */
            if(c=='\t'){ focus=(focus+1)%3; }
            else if(c=='1') focus=0;
            else if(c=='2') focus=1;
            else if(c=='3') focus=2;
            /* F1/F2/F3 are multi-byte scancodes; handled as '1'/'2'/'3' here
               as a simple stand-in. Esc -> logout. */
        }
        __asm__ volatile("hlt");
    }
}

/* ---- kernel main ---- */
void kmain(uint32_t recovery_flag){
    g_auth_user=-1;
    gdt_idt_init();
    /* enable interrupts now that IDT/PIC/PIT are ready */
    __asm__ volatile("sti");

    if(recovery_flag){
        do_recovery();                 /* may exit back to normal boot */
        /* fall through to normal boot on 'exit' */
    }

    splash();

    while(1){
        int ui = do_login();
        if(ui==-2){                    /* user picked recovery */
            do_recovery();
            continue;
        }
        if(ui<0) continue;             /* retry login */
        g_auth_user=ui;
        desktop();                    /* returns never (loops) */
    }
}
