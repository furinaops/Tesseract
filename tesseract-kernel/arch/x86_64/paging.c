#include "hyperkernel.h"

#define PAGES_PER_TABLE 1024
#define PAGE_SIZE       4096

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITE   (1 << 1)

static uint32_t *g_hk_page_dir;
static uint32_t  g_hk_page_dir_phys;

static uint32_t *alloc_zero_page(void) {
    uint32_t *p = (uint32_t *)(uintptr_t)alloc_page();
    if (!p) return 0;
    for (int i = 0; i < PAGES_PER_TABLE; i++) p[i] = 0;
    return p;
}

static uint32_t create_identity_pt(int pde_idx) {
    uint32_t *pt = alloc_zero_page();
    if (!pt) return 0;
    uint32_t base = pde_idx * 0x400000;
    for (int i = 0; i < PAGES_PER_TABLE; i++) {
        pt[i] = (base + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }
    return (uint32_t)(uintptr_t)pt;
}

void paging_init(void) {
    g_hk_page_dir = alloc_zero_page();
    if (!g_hk_page_dir) { terminal_writestring("FAILED: page dir\n"); return; }
    g_hk_page_dir_phys = (uint32_t)(uintptr_t)g_hk_page_dir;

    for (int i = 0; i < 5; i++) {
        uint32_t pt = create_identity_pt(i);
        if (!pt) { terminal_writestring("FAILED: pt\n"); return; }
        g_hk_page_dir[i] = pt | PAGE_PRESENT | PAGE_WRITE;
    }

    terminal_writestring("Paging init (4KB pages, 0-20MB identity)\n");
}

void paging_enable(void) {
    asm volatile(
        "mov %0, %%cr3\n"
        "mov %%cr0, %%eax\n"
        "orl $0x80000001, %%eax\n"
        "mov %%eax, %%cr0\n"
        : : "r"(g_hk_page_dir_phys) : "eax", "memory"
    );
    terminal_writestring("Paging enabled\n");
}

uint32_t paging_create_instance_dir(uint32_t instance_phys) {
    uint32_t *pd = alloc_zero_page();
    if (!pd) return 0;

    for (int i = 0; i < 4; i++)
        pd[i] = g_hk_page_dir[i];

    uint32_t *pt = alloc_zero_page();
    if (!pt) { free_page(pd); return 0; }

    uint32_t num = KERNEL_INSTANCES_SIZE / PAGE_SIZE;
    for (uint32_t i = 0; i < num; i++)
        pt[i] = (instance_phys + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;

    pd[4] = (uint32_t)(uintptr_t)pt | PAGE_PRESENT | PAGE_WRITE;

    return (uint32_t)(uintptr_t)pd;
}

int paging_map_instance_page(uint32_t kernel_id, uint32_t vaddr, uint32_t phys) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;

    uint32_t pd_phys = inst->page_directory;
    if (!pd_phys) return -1;

    uint32_t *pd = (uint32_t *)(uintptr_t)pd_phys;
    uint32_t pde_idx = vaddr >> 22;

    if (pde_idx != 4) return -1;

    uint32_t pt_phys = pd[4] & ~0xFFF;
    if (!pt_phys) return -1;

    uint32_t *pt = (uint32_t *)(uintptr_t)pt_phys;
    uint32_t pte_idx = (vaddr >> 12) & 0x3FF;
    pt[pte_idx] = (phys & ~0xFFF) | PAGE_PRESENT | PAGE_WRITE;

    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    return 0;
}

int paging_unmap_instance_page(uint32_t kernel_id, uint32_t vaddr) {
    kernel_instance_t *inst = get_instance(kernel_id);
    if (!inst) return -1;

    uint32_t pd_phys = inst->page_directory;
    if (!pd_phys) return -1;

    uint32_t *pd = (uint32_t *)(uintptr_t)pd_phys;
    uint32_t pde_idx = vaddr >> 22;

    if (pde_idx != 4) return -1;

    uint32_t pt_phys = pd[4] & ~0xFFF;
    if (!pt_phys) return -1;

    uint32_t *pt = (uint32_t *)(uintptr_t)pt_phys;
    uint32_t pte_idx = (vaddr >> 12) & 0x3FF;
    pt[pte_idx] = 0;

    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    return 0;
}

void paging_switch(uint32_t phys) {
    if (phys) asm volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
}

uint32_t paging_get_hypervisor_dir(void) {
    return g_hk_page_dir_phys;
}
