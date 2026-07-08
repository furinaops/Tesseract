#include "hyperkernel.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  limit_high;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct __attribute__((packed)) tss {
    uint16_t link, reserved0;
    uint32_t esp0;
    uint16_t ss0, reserved1;
    uint32_t esp1;
    uint16_t ss1, reserved2;
    uint32_t esp2;
    uint16_t ss2, reserved3;
    uint32_t cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap_base;
};

static struct gdt_entry g_gdt[6];
static struct gdt_ptr   g_gdt_ptr;
static struct tss       g_tss;

static void gdt_set_gate(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    g_gdt[idx].limit_low  = limit & 0xFFFF;
    g_gdt[idx].base_low   = base & 0xFFFF;
    g_gdt[idx].base_mid   = (base >> 16) & 0xFF;
    g_gdt[idx].access     = access;
    g_gdt[idx].limit_high = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    g_gdt[idx].base_high  = (base >> 24) & 0xFF;
}

void hypervisor_init_gdt(void) {
    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    gdt_set_gate(5, (uint32_t)(uintptr_t)&g_tss, sizeof(g_tss) - 1, 0x89, 0x40);

    g_gdt_ptr.limit = sizeof(g_gdt) - 1;
    g_gdt_ptr.base  = (uint32_t)(uintptr_t)&g_gdt;

    asm volatile("lgdt (%0)" : : "r" (&g_gdt_ptr));

    extern uint32_t g_hk_stack_top;
    g_tss.ss0  = 0x10;
    g_tss.esp0 = (uint32_t)(uintptr_t)&g_hk_stack_top;

    asm volatile("ltr %0" : : "r"((uint16_t)0x28));

    asm volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $.Lgdt_done\n"
        ".Lgdt_done:\n"
        : : : "eax"
    );

    terminal_writestring("GDT initialized\n");
}
