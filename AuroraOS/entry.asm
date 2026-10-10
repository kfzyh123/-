; =====================================================================
;  AuroraOS kernel entry (32-bit, flat binary, linked at 0x00010000)
;  This object's .text.boot section is placed first by linker.ld, so the
;  very first byte of the kernel image is _start.
; =====================================================================
[BITS 32]
[GLOBAL _start]
[EXTERN kmain]

section .text.boot
_start:
    mov  esp, 0x90000        ; set up a stack
    xor  ebp, ebp
    call kmain               ; never returns
.hang:
    cli
    hlt
    jmp .hang
