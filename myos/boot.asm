; boot.asm - 512-byte MBR bootloader for myos
; Real-mode: save drive, detect double keypress for recovery, set VGA mode 13h,
; load kernel from disk into 0x10000, enable A20, switch to 32-bit PM, jump to kernel.

[bits 16]
[org 0x7c00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [boot_drive], dl          ; BIOS passes boot drive in DL

    ; --- Recovery detection: ~1.2s window, count keypresses (>=2 -> recovery) ---
    sti
    xor cx, cx                   ; cx = key press count
    mov ah, 0x00
    int 0x1a                     ; read tick count -> dx
    mov bx, dx                   ; start tick
.wait:
    mov ah, 0x01
    int 0x16                     ; key available?
    jz .check_time
    mov ah, 0x00
    int 0x16                     ; consume key
    inc cx
    cmp cx, 2
    jae .do_recovery
    jmp .check_time
.check_time:
    mov ah, 0x00
    int 0x1a
    sub dx, bx
    cmp dx, 22                   ; ~1.2s (22 / 18.2 Hz)
    jb .wait
    jmp .load

.do_recovery:
    mov byte [recovery_flag], 1

.load:
    ; --- Load kernel: 128 sectors (64 KiB) from LBA 1 to 0x1000:0x0000 = 0x10000 ---
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .disk_err

    ; --- Set VGA mode 13h (320x200x256) before entering PM ---
    mov ax, 0x0013
    int 0x10

    ; --- Enable A20 via fast port 0x92 ---
    in al, 0x92
    or al, 2
    and al, 0xfe
    out 0x92, al

    ; --- Load GDT and enter protected mode ---
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:init_pm

.disk_err:
    mov si, msg_err
.print:
    lodsb
    or al, al
    jz .halt
    mov ah, 0x0e
    mov bx, 0x0007
    int 0x10
    jmp .print
.halt:
    hlt
    jmp .halt

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000
    movzx eax, byte [recovery_flag]
    push eax                      ; pass recovery flag to kernel
    jmp 0x10000                   ; jump to kernel entry

align 4
boot_drive:     db 0x80
recovery_flag:  db 0
msg_err:        db "DISK ERR",0

dap:
    db 0x10                       ; size of DAP
    db 0
    dw 128                        ; sector count
    dw 0x0000                     ; offset
    dw 0x1000                     ; segment -> linear 0x10000
    dq 1                          ; LBA start

gdt_start:
gdt_null:
    dq 0
gdt_code:
    dw 0xffff, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
gdt_data:
    dw 0xffff, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

times 510 - ($ - $$) db 0
dw 0xaa55
