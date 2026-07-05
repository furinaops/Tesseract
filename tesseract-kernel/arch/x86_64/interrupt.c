#include "hyperkernel.h"

#define IDT_SIZE 256

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry g_idt[IDT_SIZE];
static struct idt_ptr  g_idt_ptr;

static void idt_set_gate(int idx, uint32_t handler, uint16_t selector, uint8_t flags) {
    g_idt[idx].base_low  = handler & 0xFFFF;
    g_idt[idx].base_high = (handler >> 16) & 0xFFFF;
    g_idt[idx].selector  = selector;
    g_idt[idx].zero      = 0;
    g_idt[idx].flags     = flags;
}

static void idt_load(void) {
    asm volatile("lidt (%0)" : : "r" (&g_idt_ptr));
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void isr_default_handler(void) {
    terminal_writestring("[HYPERKERNEL] Unhandled interrupt\n");
}

extern void isr_default_wrapper(void);
extern void timer_wrapper(void);
extern void page_fault_wrapper(void);
extern void pf_recovery(void);
extern void syscall_wrapper(void);

void interrupt_init(void) {
    g_idt_ptr.limit = sizeof(g_idt) - 1;
    g_idt_ptr.base  = (uint32_t)&g_idt;

    for (int i = 0; i < IDT_SIZE; i++) {
        idt_set_gate(i, (uint32_t)isr_default_wrapper, 0x08, 0x8E);
    }

    idt_load();
    terminal_writestring("OK\n");
}

volatile uint32_t g_saved_esp;
volatile uint32_t g_switch_esp;
volatile uint32_t g_next_pd;
uint32_t g_temp_stack[8] __attribute__((used));

void timer_handler(void) {
    heartbeat_tick();

    int current = scheduler_get_current();
    if (current >= 0) {
        kernel_instance_t *inst = get_instance((uint32_t)current);
        if (inst) {
            inst->saved_esp = g_saved_esp;
            heartbeat_process_escalation(inst->id);
        }
    }

    if ((g_state.tick_count % 5) == 0) {
        uint32_t total_run = 0;
        uint32_t total_inst = 0;
        for (int i = 0; i < MAX_KERNEL_INSTANCES; i++) {
            if (g_state.instances[i].status == INSTANCE_RUNNING) {
                total_run += (uint32_t)g_state.instances[i].ticks_run;
                total_inst++;
            }
        }
        if (total_inst > 0 && total_run > 0) {
            g_state.load_factor = (total_run * 1000) / (total_inst * (uint32_t)g_state.tick_count);
            if (g_state.load_factor > 1000) g_state.load_factor = 1000;
        }
    }

    if ((g_state.tick_count % 10) == 0) {
        arbitrate_memory();
    }

    if ((g_state.tick_count % 100) == 0) {
        heartbeat_check_all();
    }

    int next_id = scheduler_next();
    kernel_instance_t *next = NULL;
    if (next_id >= 0) {
        next = get_instance((uint32_t)next_id);
    }

    if (next && next->saved_esp != 0 && next->stage != STAGE_EXECUTED) {
        g_switch_esp = next->saved_esp;
        g_next_pd = next->page_directory;
    } else {
        g_switch_esp = g_saved_esp;
        g_next_pd = 0;
    }

    outb(0x20, 0x20);
}

void page_fault_handler(void) {
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, 'P');
    outb(0x3F8, 'F');
    outb(0x3F8, ' ');
    outb(0x3F8, '@');
    outb(0x3F8, '0');
    outb(0x3F8, 'x');
    for (int i = 7; i >= 0; i--) {
        char nibble = (fault_addr >> (i * 4)) & 0xF;
        uint8_t c = (uint8_t)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        outb(0x3F8, c);
    }
    outb(0x3F8, '\r');
    outb(0x3F8, '\n');

    int current = scheduler_get_current();
    if (current >= 0) {
        destroy_instance((uint32_t)current);
    }

    paging_switch(paging_get_hypervisor_dir());

    volatile uint32_t *stack = (volatile uint32_t *)g_saved_esp;
    stack[13] = (uint32_t)pf_recovery;
    stack[14] = 0x08;
    stack[15] = 0x202;
}

static void pic_remap(void) {
    uint8_t mask_master = inb(0x21);
    uint8_t mask_slave  = inb(0xA1);

    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, mask_master & ~0x01);
    outb(0xA1, mask_slave);
}

void timer_init(void) {
    pic_remap();

    uint32_t divisor = 1193182 / 100;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    idt_set_gate(32, (uint32_t)timer_wrapper, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)page_fault_wrapper, 0x08, 0x8E);
    idt_set_gate(0x80, (uint32_t)syscall_wrapper, 0x08, 0x8E);

    terminal_writestring("OK\n");
}
