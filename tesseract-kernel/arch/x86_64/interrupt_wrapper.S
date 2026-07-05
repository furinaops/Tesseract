.section .text

.extern g_saved_esp
.extern g_switch_esp
.extern g_next_pd
.extern g_temp_stack

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
  movl %esp, g_saved_esp
  cld
  call timer_handler
  lea g_temp_stack + 32, %esp
  movl g_next_pd, %eax
  test %eax, %eax
  jz 0f
  mov %eax, %cr3
0:
  movl g_switch_esp, %esp
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
  movl %esp, g_saved_esp
  cld
  call page_fault_handler
  pop %gs
  pop %fs
  pop %es
  pop %ds
  popal
  add $4, %esp
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
  push %edx
  push %ecx
  push %ebx
  push %eax
  cld
  call syscall_handler_c
  add $16, %esp
  movl %eax, 44(%esp)
  pop %gs
  pop %fs
  pop %es
  pop %ds
  popal
  iret
