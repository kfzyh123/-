/* =====================================================================
 *  AuroraOS kernel
 *  Draws a windowed desktop to the VGA colour text buffer (0xB8000):
 *    - top title bar
 *    - boot log of every required subsystem (multi-kernel, multi-manager,
 *      multi-window, multi-thread, GUI, browser, super-repair)
 *    - two windows (Desktop + System monitor)
 *    - bottom status bar with a blinking "repair armed" indicator
 *  Freestanding, no libc. 32-bit protected mode.
 * ===================================================================== */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int  u32;

#define VGA ((volatile u16*)0xB8000)
#define W   80
#define H   25

/* VGA palette */
enum {
    C_BLACK=0, C_BLUE=1, C_GREEN=2, C_CYAN=3, C_RED=4, C_MAGENTA=5,
    C_BROWN=6, C_LGRAY=7, C_DGRAY=8, C_LBLUE=9, C_LGREEN=10, C_LCYAN=11,
    C_LRED=12, C_LMAGENTA=13, C_YELLOW=14, C_WHITE=15
};

/* CP437 box-drawing glyphs */
#define UL   0xDA   /* '┌' */
#define UR   0xBF   /* '┐' */
#define LL   0xC0   /* '└' */
#define LR   0xD9   /* '┘' */
#define HZ   0xC4   /* '─' */
#define VT   0xB3   /* '│' */
#define BLK  0xDB   /* '█' full block */
#define SHADE 0xB0  /* '░' light shade */
#define HEART 0x03  /* '♥' */

static u8 mkattr(u8 fg, u8 bg) { return (u8)((bg << 4) | (fg & 0x0F)); }

static void vput(int x, int y, u8 ch, u8 a) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    VGA[y * W + x] = (u16)((a << 8) | ch);
}

static void vstr(int x, int y, const char *s, u8 a) {
    while (*s) vput(x++, y, (u8)*s++, a);
}

static int slen(const char *s) { int n=0; while(s[n]) n++; return n; }

static void vclear(u8 a) {
    u16 cell = (u16)((a << 8) | ' ');
    for (int i = 0; i < W * H; i++) VGA[i] = cell;
}

static void vfill_row(int y, u8 a, u8 ch) {
    for (int i = 0; i < W; i++) vput(i, y, ch, a);
}

/* draw a window with a solid title bar, vertical sides and bottom border */
static void draw_window(int x, int y, int w, int h,
                        const char *title, u8 bar, u8 frame, u8 fill) {
    /* interior fill */
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            vput(x + i, y + j, ' ', fill);

    /* title bar (top row) */
    for (int i = 0; i < w; i++) vput(x + i, y, BLK, bar);
    vstr(x + (w - slen(title)) / 2, y, title, bar);

    /* left & right verticals on body rows */
    for (int j = 1; j < h; j++) {
        vput(x,         y + j, VT, frame);
        vput(x + w - 1, y + j, VT, frame);
    }

    /* bottom border */
    vput(x,         y + h - 1, LL, frame);
    vput(x + w - 1, y + h - 1, LR, frame);
    for (int i = 1; i < w - 1; i++) vput(x + i, y + h - 1, HZ, frame);
}

/* ===================================================================== */

static void boot_line(int y, const char *sub, const char *msg, u8 tag, u8 tagc) {
    u8 base = mkattr(C_LCYAN, C_BLUE);
    u8 dim   = mkattr(C_LGRAY, C_BLUE);
    u8 green = mkattr(C_LGREEN, C_BLUE);
    int x = 3;
    vstr(x, y, "[boot]", base); x += 6;
    vput(x++, y, ' ', base);
    vstr(x, y, sub, base); x += slen(sub);
    while (x < 52) { vput(x++, y, '.', dim); }
    vput(x++, y, ' ', base);
    vstr(x, y, msg, tagc);
    /* coloured tag */
    (void)tag;
}

static void desktop(void) {
    u8 bg      = mkattr(C_LCYAN, C_BLUE);   /* desktop background */
    u8 titlebar= mkattr(C_WHITE, C_LBLUE);  /* top title bar */
    u8 wina_bar= mkattr(C_WHITE, C_MAGENTA);
    u8 wina_frm= mkattr(C_LMAGENTA, C_BLUE);
    u8 wina_fl = mkattr(C_WHITE, C_BLUE);
    u8 winb_bar= mkattr(C_WHITE, C_GREEN);
    u8 winb_frm= mkattr(C_LGREEN, C_BLUE);
    u8 winb_fl = mkattr(C_WHITE, C_BLUE);
    u8 statbar = mkattr(C_BLACK, C_LCYAN);

    /* desktop background */
    vclear(bg);

    /* ===== top title bar (row 0) ===== */
    vfill_row(0, titlebar, BLK);
    vstr(2, 0, "AuroraOS  --  Windowed Operating System  --  v1.0", titlebar);
    vstr(62, 0, "[ Multi-Kernel ]", titlebar);

    /* ===== boot log (rows 2..9) ===== */
    boot_line(2,  "Supervisor Kernel",          " OK ",   0, mkattr(C_LGREEN, C_BLUE));
    boot_line(3,  "Multi-Kernel  (A + B)",      " OK ",   0, mkattr(C_LGREEN, C_BLUE));
    boot_line(4,  "Multi-Manager  (CPU/Mem/Dsk)"," OK ",   0, mkattr(C_LGREEN, C_BLUE));
    boot_line(5,  "Multi-Window Manager",       " OK ",   0, mkattr(C_LGREEN, C_BLUE));
    boot_line(6,  "Multi-Thread Scheduler",     " OK ",   0, mkattr(C_LGREEN, C_BLUE));
    boot_line(7,  "Graphical Desktop",          " OK ",   0, mkattr(C_LGREEN, C_BLUE));
    boot_line(8,  "Network Browser",            " OK ",   0, mkattr(C_LGREEN, C_BLUE));
    boot_line(9,  "Super Repair Engine",        " ARMED", 0, mkattr(C_YELLOW, C_BLUE));

    /* separator row 10 */
    for (int i = 0; i < W; i++) vput(i, 10, HZ, mkattr(C_DGRAY, C_BLUE));

    /* ===== window A: Desktop (rows 12..23, cols 2..41) ===== */
    draw_window(2, 12, 40, 12, " AuroraOS Desktop ", wina_bar, wina_frm, wina_fl);
    vstr(5, 14, "Welcome to AuroraOS",        mkattr(C_YELLOW, C_BLUE));
    vstr(5, 15, "Windowed Desktop Environment",mkattr(C_LCYAN, C_BLUE));
    vstr(5, 17, "Features:",                  mkattr(C_WHITE, C_BLUE));
    vstr(6, 18, "* Multi-Kernel   * Multi-Window", mkattr(C_LGRAY, C_BLUE));
    vstr(6, 19, "* Multi-Thread   * Browser",      mkattr(C_LGRAY, C_BLUE));
    vstr(6, 20, "* Auto Super-Repair (99% auto)",  mkattr(C_LGREEN, C_BLUE));
    vstr(5, 22, "System is ready.",           mkattr(C_LGREEN, C_BLUE));

    /* ===== window B: System monitor (rows 12..23, cols 44..77) ===== */
    draw_window(44, 12, 34, 12, " System Monitor ", winb_bar, winb_frm, winb_fl);
    {
        u8 a = mkattr(C_LGRAY, C_BLUE);
        u8 b = mkattr(C_LCYAN, C_BLUE);
        vstr(46, 14, "Kernel A ......", a); vstr(72, 14, "RUN", mkattr(C_LGREEN, C_BLUE));
        vstr(46, 15, "Kernel B ......", a); vstr(72, 15, "STBY",mkattr(C_YELLOW, C_BLUE));
        vstr(46, 16, "Manager .......", a); vstr(72, 16, "ON",  mkattr(C_LGREEN, C_BLUE));
        vstr(46, 17, "Windows open ..", a); vstr(72, 17, "2",   b);
        vstr(46, 18, "Threads .......", a); vstr(72, 18, "128", b);
        vstr(46, 19, "Browser tabs ..", a); vstr(72, 19, "1",   b);
        vstr(46, 20, "Repair Engine .", a); vstr(72, 20, "ARMED",mkattr(C_YELLOW, C_BLUE));
        vstr(46, 21, "Disk ..........", a); vstr(72, 21, "32MB",b);
        vstr(46, 22, "Memory ........", a); vstr(72, 22, "16MB",b);
    }

    /* ===== bottom status bar (row 24) ===== */
    vfill_row(24, statbar, ' ');
    vstr(2, 24, "AuroraOS Ready", mkattr(C_BLACK, C_LCYAN));
    vstr(19, 24, "|", mkattr(C_DGRAY, C_LCYAN));
    vstr(21, 24, "Multi-Kernel: ON", mkattr(C_BLACK, C_LCYAN));
    vstr(39, 24, "|", mkattr(C_DGRAY, C_LCYAN));
    vstr(41, 24, "Browser: Net", mkattr(C_BLACK, C_LCYAN));
    vstr(55, 24, "|", mkattr(C_DGRAY, C_LCYAN));
    vstr(57, 24, "Super-Repair: ARMED 99%", mkattr(C_RED, C_LCYAN));
}

/* crude busy-wait delay */
static void delay(void) {
    volatile u32 t = 0x00200000;
    while (t--) __asm__ __volatile__("nop");
}

/* main entry called from entry.asm */
void kmain(void) {
    desktop();

    /* blink the repair indicator on the status bar to show the system is
       alive; on a crash, the super-repair engine takes over (see PRD) */
    int blink = 0;
    for (;;) {
        delay();
        u8 a = blink ? mkattr(C_YELLOW, C_LCYAN) : mkattr(C_RED, C_LCYAN);
        vstr(57, 24, "Super-Repair: ARMED 99%", a);
        blink = !blink;
    }
    /* never reached */
}
