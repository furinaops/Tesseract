.section .text
.global _start
_start:
    mov $kernel_stack_top, %esp
    call kernel_main
    cli
.Lhang:
    hlt
    jmp .Lhang

.section .bss
.align 16
.skip 4096
kernel_stack_top:
