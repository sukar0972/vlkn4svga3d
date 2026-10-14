/* SPDX-License-Identifier: GPL-2.0-or-later */
.section .multiboot,"a"
.align 4
.long 0x1badb002, 0, -0x1badb002
.section .text
.code32
.global _start
_start:
    mov $stack_top, %esp
    call guest_main
1:  cli
    hlt
    jmp 1b
.section .bss
.align 16
.space 16384
stack_top:
.section .note.GNU-stack,"",@progbits
