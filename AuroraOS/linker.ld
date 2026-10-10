/* AuroraOS kernel linker script - link as flat binary at 0x10000 */
ENTRY(_start)

SECTIONS
{
    . = 0x00010000;

    /* entry.asm's .text.boot is forced first so _start is byte 0 */
    .text : {
        *(.text.boot)
        *(.text)
        *(.text.*)
    }
    .rodata : { *(.rodata) *(.rodata.*) }
    .data : { *(.data) *(.data.*) }
    .bss : { *(.bss) *(.bss.*) *(COMMON) }

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note.*) *(.note_*) }
}
