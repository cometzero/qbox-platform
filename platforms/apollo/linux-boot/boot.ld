/* SPDX-License-Identifier: Apache-2.0 */
ENTRY(_start)
SECTIONS
{
    . = 0x000f0000;
    .text : { KEEP(*(.text.boot)) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    ASSERT(SIZEOF(.bss) == 0, "Reset payload requires BSS initialization")
    ASSERT(. <= 0x000ff000, "Reset payload overlaps its 4 KiB SRAM stack")
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
