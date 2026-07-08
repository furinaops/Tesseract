#include "hyperkernel.h"

#define IDT_SIZE 256
#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITE   (1 << 1)
#define PAGE_USER    (1 << 2)
#define PAGE_SIZE    4096

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
extern void gp_fault_wrapper(void);
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
uint32_t g_temp_stack[64] __attribute__((used));

volatile uint32_t g_hk_cr3_imm_timer __attribute__((used)) = 0;
volatile uint32_t g_hk_cr3_imm_pf __attribute__((used)) = 0;
volatile uint32_t g_hk_cr3_imm_syscall __attribute__((used)) = 0;
volatile uint32_t g_current_instance_cr3 __attribute__((used)) = 0;

extern volatile uint32_t g_hk_stack_canary;
#define STACK_CANARY_VALUE 0xDEADBEEF

void timer_handler(void) {
    if (g_hk_stack_canary != STACK_CANARY_VALUE) {
        PANIC("Stack canary corrupted");
    }

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
        uint32_t *pd = (uint32_t *)(uintptr_t)next->page_directory;
        uint32_t *pt4 = (uint32_t *)(uintptr_t)next->pt4_phys;
        int corrupted = 0;
        uint32_t num = KERNEL_INSTANCES_SIZE / PAGE_SIZE;
        for (uint32_t i = 0; i < num; i++) {
            uint32_t expected = (next->memory_base + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
            if (pt4[i] != expected) { corrupted = 1; break; }
        }
        if ((pd[4] & ~0xFFF) != next->pt4_phys) corrupted = 1;
        if ((pd[4] & (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)) != (PAGE_PRESENT | PAGE_WRITE | PAGE_USER))
            corrupted = 1;

        if (corrupted) {
            destroy_instance(next->id);
            g_switch_esp = g_saved_esp;
            g_next_pd = 0;
            g_current_instance_cr3 = 0;
        } else {
            g_switch_esp = next->saved_esp;
            g_next_pd = next->page_directory;
            g_current_instance_cr3 = next->page_directory;
            paging_set_hk_pd4(next->page_directory);
        }
    } else {
        g_switch_esp = g_saved_esp;
        g_next_pd = 0;
    }

    outb(0x20, 0x20);
}

static void pf_puthex(uint32_t v) {
    for (int i = 7; i >= 0; i--) {
        char nibble = (v >> (i * 4)) & 0xF;
        uint8_t c = (uint8_t)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        outb(0x3F8, c);
    }
}

void page_fault_handler(void) {
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr) : : "memory");

    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, 'P');
    outb(0x3F8, 'F');
    outb(0x3F8, ' ');
    pf_puthex(fault_addr);
    outb(0x3F8, ' ');
    outb(0x3F8, 'e');
    outb(0x3F8, '=');
    pf_puthex(g_saved_esp);
    outb(0x3F8, '\r');
    outb(0x3F8, '\n');

    int current = scheduler_get_current();
    if (current >= 0) {
        destroy_instance((uint32_t)current);
    }

    paging_reset_hk_pd4();
}

static void gp_putchar(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0) {}
    outb(0x3F8, (uint8_t)c);
}

void gp_fault_handler(void) {
    gp_putchar('G');
    gp_putchar('P');
    gp_putchar('#');
    gp_putchar('\r');
    gp_putchar('\n');
    int current = scheduler_get_current();
    if (current >= 0) {
        destroy_instance((uint32_t)current);
    }
    paging_reset_hk_pd4();
}

void patch_cr3_values(void) {
    uint32_t hk_cr3 = paging_get_hypervisor_dir();
    g_hk_cr3_imm_timer = hk_cr3;
    g_hk_cr3_imm_pf = hk_cr3;
    g_hk_cr3_imm_syscall = hk_cr3;
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

    idt_set_gate(13, (uint32_t)gp_fault_wrapper, 0x08, 0x8E);
    idt_set_gate(32, (uint32_t)timer_wrapper, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)page_fault_wrapper, 0x08, 0x8E);
    idt_set_gate(0x80, (uint32_t)syscall_wrapper, 0x08, 0xEE);

    terminal_writestring("OK\n");
}
