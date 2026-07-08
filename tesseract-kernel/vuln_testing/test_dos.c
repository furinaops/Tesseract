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

typedef struct {
    uint32_t memory_footprint;
    uint32_t memory_request;
    uint32_t health_score;
    uint32_t last_syscall_low;
} heartbeat_payload_t;

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

    swrite("DOS"); swritedec(my_id); swrite(" start\n");

    heartbeat_payload_t payload;
    payload.memory_footprint = 64;
    payload.memory_request = 64;
    payload.health_score = 100;

    uint32_t burst_count = 0;
    int last_result = 0;

    swrite("DOS Phase 1: rapid burst...\n");

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
            swrite("DOS burst "); swritedec(i);
            swrite(" ret="); swritedec((uint32_t)(int32_t)result);
            swrite("\n");
        }
    }

    swrite("DOS Phase 1 done. Burst count="); swritedec(burst_count);
    swrite(" last_ret="); swritedec((uint32_t)(int32_t)last_result);
    swrite("\n");

    swrite("DOS Phase 2: normal rate...\n");

    for (int i = 0; i < 10; i++) {
        int result;
        asm volatile("int $0x80"
                     : "=a"(result)
                     : "a"(5), "b"(&payload)
                     : "ecx", "edx", "memory");

        swrite("DOS normal "); swritedec(i);
        swrite(" ret="); swritedec((uint32_t)(int32_t)result);
        swrite("\n");

        /* ~200ms delay via busy-wait (rough) */
        for (volatile uint32_t d = 0; d < 500000; d++);
    }

    swrite("DOS done. System stable.\n");

    for (;;) { for (volatile int _i = 0; _i < 5000000; _i++); }
}
