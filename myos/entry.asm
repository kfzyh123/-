; entry.asm - kernel entry point + ISR stubs (nasm, elf32)
; _start reads the recovery flag pushed by the bootloader, then calls kmain().

[BITS 32]
[SECTION .text.boot]

global _start
global load_gdt
global load_idt
global isr_default
global irq_timer
global irq_keyboard

extern kmain
extern isr_default_handler
extern irq_timer_handler
extern irq_keyboard_handler

_start:
    pop eax                        ; eax = recovery flag (pushed by bootloader)
    mov esp, 0x90000
    push eax
    call kmain
.hang:
    cli
    hlt
    jmp .hang

load_gdt:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

load_idt:
    mov eax, [esp+4]
    lidt [eax]
    ret

; Default CPU exception handler stub. CPU pushes an error code only for some
; vectors; we push a dummy 0 for uniformity. We never iret back: the C handler
; enters the recovery environment and halts there.
isr_default:
    push 0
    pusha
    push ds
    push es
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    cld
    call isr_default_handler
    pop es
    pop ds
    popa
    add esp, 4
    iret

; IRQ0 - timer (PIT ~100Hz). Drives the cooperative scheduler.
irq_timer:
    pusha
    push ds
    mov ax, 0x10
    mov ds, ax
    cld
    call irq_timer_handler
    mov al, 0x20
    out 0x20, al                   ; EOI master PIC
    pop ds
    popa
    iret

; IRQ1 - keyboard (PS/2 scancode set 1).
irq_keyboard:
    pusha
    push ds
    mov ax, 0x10
    mov ds, ax
    cld
    call irq_keyboard_handler
    mov al, 0x20
    out 0x20, al                   ; EOI master PIC
    pop ds
    popa
    iret
