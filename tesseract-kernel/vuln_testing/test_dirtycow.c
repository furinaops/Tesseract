/*
 * test_dirtycow.c — "Dirty COW" Memory Isolation Test
 *
 * In the Tesseract hyperkernel, PDE 0–3 are identity-mapped and
 * *shared* across all kernel instances. This means any instance can
 * read/write any other instance's memory through the identity map.
 *
 * This test demonstrates the vulnerability:
 *   1. Instance 1 writes a marker string into instance 2's VGA row
 *      via the identity-mapped physical address.
 *   2. System logs the result.
 *
 * Expected result: writes succeed → memory isolation FAILURE logged.
 * If the hyperkernel had proper isolation, the write would page-fault.
 */

#include <stdint.h>

#define VGA_ADDR    ((volatile uint16_t *)0xB8000)
#define VGA_WIDTH   80
#define SERIAL_PORT 0x3F8

static void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_putchar(char c) {
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0);
    outb(SERIAL_PORT, (uint8_t)c);
}

static void serial_writestring(const char *str) {
    for (; *str; str++) {
        if (*str == '\n') serial_putchar('\r');
        serial_putchar(*str);
    }
}

static void serial_writehex(uint32_t n) {
    for (int i = 7; i >= 0; i--) {
        char nib = (n >> (i * 4)) & 0xF;
        serial_putchar(nib < 10 ? '0' + nib : 'A' + nib - 10);
    }
}

void kernel_main(void) {
    uint32_t my_id = *(volatile uint32_t *)0x100FFC0;

    serial_writestring("DCW");
    serial_writehex(my_id);
    serial_writestring(" start\n");

    /*
     * Calculate the VGA row for another instance (victim).
     * Each instance writes to row = (id-1) * 6 in kernel_template.
     * Instance 0 uses row 0. We'll try to overwrite it.
     *
     * Identity mapping: virtual 0xB8000 = physical 0xB8000.
     * All instances share PDE 0 (0-4MB), so this write goes through.
     */

    uint32_t victim_id = (my_id == 1) ? 2 : 1;
    int victim_row = (victim_id - 1) * 6;
    int offset = victim_row * VGA_WIDTH;

    serial_writestring("DCW overwriting victim VGA row via identity map...\n");

    const char *marker = "DIRTYCOW";
    for (int i = 0; marker[i]; i++) {
        VGA_ADDR[offset + i] = (uint16_t)marker[i] | (uint16_t)0x04 << 8;
    }

    serial_writestring("DCW write completed. Isolation FAILED (shared PDE 0-3).\n");

    for (;;) asm volatile("hlt");
}
