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

static void sput(char c) {
    asm volatile("int $0x80" : : "a"(6), "b"((uint32_t)c) : "ecx", "edx", "memory");
}
static void swrite(const char *str) {
    for (; *str; str++) { if (*str == '\n') sput('\r'); sput(*str); }
}
static void swritedec(uint32_t n) {
    char buf[12]; int i = 11; buf[11] = '\0';
    if (n == 0) { sput('0'); return; }
    while (n > 0 && i > 0) { i--; buf[i] = '0' + (n % 10); n /= 10; }
    swrite(&buf[i]);
}

void kernel_main(void) {
    uint32_t my_id = *(volatile uint32_t *)0x100FFB0;

    swrite("OVF"); swritedec(my_id); swrite(" start\n");

    swrite("OVF attempting write to 0x10010000...\n");
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0x10010000;
    *bad_ptr = 0xDEADBEEF;

    swrite("OVF SURVIVED! Isolation FAILED.\n");

    for (;;) { for (volatile int _i = 0; _i < 5000000; _i++); }
}
