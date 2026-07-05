/*
 * test_overflow.c — Buffer Overflow Test
 *
 * Attempts to write past the instance's 64KB PDE-4 boundary into
 * unmapped PDE-5 space (virtual 0x10010000+). The hyperkernel should
 * catch the resulting page fault, destroy the instance, and log it.
 *
 * Expected result: page fault → instance destroyed → "PF @0x..."
 * logged to serial. System stays up.
 */

#include <stdint.h>

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

static void serial_writedec(uint32_t n) {
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    if (n == 0) { serial_putchar('0'); return; }
    while (n > 0 && i > 0) {
        i--;
        buf[i] = '0' + (n % 10);
        n /= 10;
    }
    serial_writestring(&buf[i]);
}

void kernel_main(void) {
    uint32_t my_id = *(volatile uint32_t *)0x100FFC0;

    serial_writestring("OVF");
    serial_writedec(my_id);
    serial_writestring(" start\n");

    /* Attempt 1: write just past PDE-4 (virtual 0x10010000, unmapped) */
    serial_writestring("OVF attempting write to 0x10010000...\n");
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0x10010000;
    *bad_ptr = 0xDEADBEEF;

    /* If we survive, report success (should not happen) */
    serial_writestring("OVF SURVIVED! Isolation FAILED.\n");

    for (;;) asm volatile("hlt");
}
