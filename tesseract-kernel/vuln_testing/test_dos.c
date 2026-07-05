/*
 * test_dos.c — Denial of Service (Rapid Syscall) Test
 *
 * Spams syscall 5 (heartbeat) in a tight loop with no hlt, attempting
 * to overwhelm the hyperkernel's syscall handler. The hyperkernel's
 * rate limiter should reject calls faster than 1 per 50ms (5 ticks).
 *
 * Expected result: syscalls are rate-limited, hyperkernel stays stable,
 * no crash. If the rate limiter fails, the system may hang or crash.
 */

#include <stdint.h>

#define SERIAL_PORT 0x3F8

typedef struct {
    uint32_t memory_footprint;
    uint32_t memory_request;
    uint32_t health_score;
    uint32_t last_syscall_low;
} heartbeat_payload_t;

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

    serial_writestring("DOS");
    serial_writedec(my_id);
    serial_writestring(" start\n");

    heartbeat_payload_t payload;
    payload.memory_footprint = 64;
    payload.memory_request = 64;
    payload.health_score = 100;

    uint32_t burst_count = 0;
    int last_result = 0;

    /* Phase 1: Rapid burst — syscall every iteration, no hlt */
    serial_writestring("DOS Phase 1: rapid burst (no hlt)...\n");

    for (int i = 0; i < 1000; i++) {
        int result;
        asm volatile("int $0x80"
                     : "=a"(result)
                     : "a"(5), "b"(&payload)
                     : "ecx", "edx", "memory");
        burst_count++;
        last_result = result;

        /* Read result every 100 iterations to reduce serial noise */
        if ((i % 100) == 0) {
            serial_writestring("DOS burst ");
            serial_writedec(i);
            serial_writestring(" ret=");
            serial_writedec((uint32_t)(int32_t)result);
            serial_writestring("\n");
        }
    }

    serial_writestring("DOS Phase 1 done. Burst count=");
    serial_writedec(burst_count);
    serial_writestring(" last_ret=");
    serial_writedec((uint32_t)(int32_t)last_result);
    serial_writestring("\n");

    /* Phase 2: Slow rate — ensure normal syscalls still work */
    serial_writestring("DOS Phase 2: normal rate...\n");

    for (int i = 0; i < 10; i++) {
        int result;
        asm volatile("int $0x80"
                     : "=a"(result)
                     : "a"(5), "b"(&payload)
                     : "ecx", "edx", "memory");

        serial_writestring("DOS normal ");
        serial_writedec(i);
        serial_writestring(" ret=");
        serial_writedec((uint32_t)(int32_t)result);
        serial_writestring("\n");

        /* ~200ms delay via busy-wait (rough) */
        for (volatile uint32_t d = 0; d < 500000; d++);
    }

    serial_writestring("DOS done. System stable.\n");

    for (;;) asm volatile("hlt");
}
