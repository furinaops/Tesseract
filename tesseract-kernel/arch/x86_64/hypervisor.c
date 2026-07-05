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

static struct gdt_entry g_gdt[5];
static struct gdt_ptr   g_gdt_ptr;

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

    g_gdt_ptr.limit = sizeof(g_gdt) - 1;
    g_gdt_ptr.base  = (uint32_t)(uintptr_t)&g_gdt;

    asm volatile("lgdt (%0)" : : "r" (&g_gdt_ptr));
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
