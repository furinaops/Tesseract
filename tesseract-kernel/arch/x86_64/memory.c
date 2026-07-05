#include "hyperkernel.h"

#define PAGE_SIZE      4096
#define MAX_PAGES      1024
#define MEMORY_START   0x200000
#define MEMORY_END     0x800000

static uint8_t g_page_bitmap[MAX_PAGES / 8];
static uint32_t g_next_free_page;

static void bitmap_set(int page) {
    g_page_bitmap[page / 8] |= (1 << (page % 8));
}

static void bitmap_clear(int page) {
    g_page_bitmap[page / 8] &= ~(1 << (page % 8));
}

static int bitmap_test(int page) {
    return (g_page_bitmap[page / 8] >> (page % 8)) & 1;
}

void memory_init(void) {
    for (int i = 0; i < MAX_PAGES / 8; i++) {
        g_page_bitmap[i] = 0;
    }
    g_next_free_page = 0;
}

void *alloc_page(void) {
    for (uint32_t i = g_next_free_page; i < MAX_PAGES; i++) {
        if (!bitmap_test((int)i)) {
            bitmap_set((int)i);
            g_next_free_page = i + 1;
            return (void *)(uintptr_t)(MEMORY_START + i * PAGE_SIZE);
        }
    }
    for (uint32_t i = 0; i < g_next_free_page; i++) {
        if (!bitmap_test((int)i)) {
            bitmap_set((int)i);
            g_next_free_page = i + 1;
            return (void *)(uintptr_t)(MEMORY_START + i * PAGE_SIZE);
        }
    }
    return 0;
}

void free_page(void *addr) {
    uint32_t page = ((uint32_t)(uintptr_t)addr - MEMORY_START) / PAGE_SIZE;
    if (page < MAX_PAGES) {
        bitmap_clear((int)page);
        if (page < g_next_free_page) {
            g_next_free_page = page;
        }
    }
}

void *alloc_pages(uint32_t count) {
    if (count == 0) return 0;
    uint32_t start = 0;
    uint32_t consecutive = 0;
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (!bitmap_test((int)i)) {
            if (consecutive == 0) start = i;
            consecutive++;
            if (consecutive == count) {
                for (uint32_t j = start; j < start + count; j++) {
                    bitmap_set((int)j);
                }
                return (void *)(uintptr_t)(MEMORY_START + start * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }
    return 0;
}

void free_pages(void *addr, uint32_t count) {
    uint32_t start = ((uint32_t)(uintptr_t)addr - MEMORY_START) / PAGE_SIZE;
    for (uint32_t i = start; i < start + count && i < MAX_PAGES; i++) {
        bitmap_clear((int)i);
    }
}
