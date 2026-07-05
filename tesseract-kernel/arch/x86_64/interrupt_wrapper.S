.section .text

.extern g_saved_esp
.extern g_switch_esp
.extern g_next_pd
.extern g_temp_stack
.extern g_hk_cr3_imm_timer
.extern g_hk_cr3_imm_pf
.extern g_hk_cr3_imm_syscall
.extern g_current_instance_cr3
.extern timer_handler
.extern page_fault_handler
.extern syscall_handler_c
.extern pf_recovery

.section .bss
.align 16
g_hk_stack_bottom:
    .space 1024
g_hk_stack_top:
g_hk_payload_copy:
    .space 16

.section .text

.globl isr_default_wrapper
isr_default_wrapper:
    pushal
    call isr_default_handler
    popal
    iret

.globl timer_wrapper
timer_wrapper:
    pushal
    push %ds
    push %es
    push %fs
    push %gs
    mov %esp, %ebx
    lea g_hk_stack_top, %esp
    movl (g_hk_cr3_imm_timer), %eax
    mov %eax, %cr3
    mov %ebx, g_saved_esp
    cld
    call timer_handler
    lea g_temp_stack + 32, %esp
    movl g_next_pd, %eax
    movl g_switch_esp, %ecx
    test %eax, %eax
    jz 0f
    mov %eax, %cr3
0:
    mov %ecx, %esp
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popal
    iret

.globl page_fault_wrapper
page_fault_wrapper:
    pushal
    push %ds
    push %es
    push %fs
    push %gs
    mov %esp, %ebx
    lea g_hk_stack_top, %esp
    movl (g_hk_cr3_imm_pf), %eax
    mov %eax, %cr3
    mov %ebx, g_saved_esp
    cld
    call page_fault_handler
    pushl $0x202
    pushl $0x08
    pushl $pf_recovery
    iret

.globl pf_recovery
pf_recovery:
    sti
1:
    hlt
    jmp 1b

.globl syscall_wrapper
syscall_wrapper:
    pushal
    push %ds
    push %es
    push %fs
    push %gs
    mov %esp, %ebx
    cmpl $5, 44(%ebx)
    jne 1f
    mov 32(%ebx), %esi
    lea g_hk_payload_copy, %edi
    mov 0(%esi), %eax
    mov %eax, 0(%edi)
    mov 4(%esi), %eax
    mov %eax, 4(%edi)
    mov 8(%esi), %eax
    mov %eax, 8(%edi)
    mov 12(%esi), %eax
    mov %eax, 12(%edi)
    mov %edi, 32(%ebx)
1:
    mov 44(%ebx), %eax
    mov 32(%ebx), %esi
    mov 40(%ebx), %ecx
    mov 36(%ebx), %edx
    lea g_hk_stack_top, %esp
    movl (g_hk_cr3_imm_syscall), %edi
    mov %edi, %cr3
    mov %ebx, g_saved_esp
    push %edx
    push %ecx
    push %esi
    push %eax
    cld
    call syscall_handler_c
    add $16, %esp
    movl (g_current_instance_cr3), %ecx
    mov %ecx, %cr3
    mov %ebx, %esp
    mov %eax, 44(%esp)
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popal
    iret
