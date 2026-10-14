/* SPDX-License-Identifier: GPL-2.0-or-later */
ENTRY(_start)
SECTIONS {
    . = 1M;
    .multiboot : { *(.multiboot) }
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
