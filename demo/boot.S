.section .multiboot, "a", @progbits
.align 4
.long 0x1BADB002
.long 0x00000003
.long -(0x1BADB002 + 0x00000003)

.section .text
.global _start
_start:
    mov $stack_top, %esp
    call kernel_main
    cli
.Lhang:
    hlt
    jmp .Lhang

.section .bss
.align 16
.skip 16384
stack_top:
