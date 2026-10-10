; =====================================================================
;  AuroraOS  -  512-byte boot sector (stage 1)
;  16-bit real mode -> loads kernel -> enables A20 -> enters 32-bit
;  protected mode -> far-jumps into the kernel at 0x00100000... NO,
;  at linear 0x00010000 (64KB), where the flat kernel binary lives.
; =====================================================================
[BITS 16]
[ORG 0x7C00]

; Where the kernel will be loaded (segment 0x1000 -> linear 0x10000)
KERNEL_SEG    equ 0x1000
; How many 512-byte sectors of kernel to read from disk (sector 2..)
; 48 sectors = 24 KiB, plenty for this kernel.
KERNEL_SECTORS equ 48

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; BIOS leaves the boot drive number in DL
    mov [boot_drive], dl

    ; Greet
    mov si, msg_boot
    call print_str

    ; --- read kernel sectors into 0x1000:0000 (= linear 0x10000) ---
    mov ax, KERNEL_SEG
    mov es, ax
    xor bx, bx           ; ES:BX = 0x1000:0000

    mov ah, 0x02         ; BIOS read sectors
    mov al, KERNEL_SECTORS
    mov ch, 0x00         ; cylinder 0
    mov cl, 0x02         ; start at sector 2 (sector 1 = this boot sector)
    mov dh, 0x00         ; head 0
    mov dl, [boot_drive]
    int 0x13
    jc  disk_err         ; CF set on error

    mov si, msg_ok
    call print_str

    ; --- enable A20 line (fast A20 via system port 0x92) ---
    in   al, 0x92
    or   al, 0x02
    out  0x92, al

    ; --- load GDT and switch to protected mode ---
    cli
    lgdt [gdt_desc]

    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax

    ; far jump flushes the prefetch queue and loads the code selector
    jmp  CODE_SEG:init_pm

disk_err:
    mov si, msg_err
    call print_str
    ; hang forever
.hang:
    hlt
    jmp .hang

; ---- print null-terminated string at DS:SI via BIOS teletype ----
print_str:
    lodsb
    test al, al
    jz  .ret
    mov ah, 0x0E         ; teletype
    mov bx, 0x0007       ; page 0, attribute 07h (ignored in most modes)
    int 0x10
    jmp print_str
.ret:
    ret

; =====================================================================
;  32-bit protected mode entry (still inside the 512-byte boot sector)
; =====================================================================
[BITS 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000      ; stack well below the kernel
    jmp 0x10000           ; jump to the kernel's first byte

; =====================================================================
;  Global Descriptor Table - flat memory model
; =====================================================================
gdt_start:
    dd 0, 0                          ; null descriptor
gdt_code:                           ; 0x08 - 32-bit code, base 0, limit 4 GiB
    dw 0xFFFF, 0x0000
    db 0x00, 0x9A, 0xCF, 0x00
gdt_data:                           ; 0x10 - data, base 0, limit 4 GiB
    dw 0xFFFF, 0x0000
    db 0x00, 0x92, 0xCF, 0x00
gdt_end:

gdt_desc:
    dw  gdt_end - gdt_start - 1
    dd  gdt_start

CODE_SEG  equ gdt_code - gdt_start    ; 0x08
DATA_SEG  equ gdt_data - gdt_start    ; 0x10

; =====================================================================
;  data
; =====================================================================
boot_drive: db 0
msg_boot:  db "AuroraOS stage1: booting...",  0
msg_ok:    db " kernel loaded, entering PM.", 0
msg_err:   db "DISK READ ERROR", 0

; ---- pad to 510 and add the boot signature ----
times 510-($-$$) db 0
dw 0xAA55
