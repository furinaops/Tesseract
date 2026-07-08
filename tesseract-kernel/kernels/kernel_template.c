#include <stdint.h>

typedef struct {
    uint32_t memory_footprint;
    uint32_t memory_request;
    uint32_t health_score;
    uint32_t last_syscall_low;
} heartbeat_payload_t;

static void heartbeat(void) {
    heartbeat_payload_t payload;
    payload.memory_footprint = 64;
    payload.memory_request = 64;
    payload.health_score = 100;
    asm volatile("mov $5, %%eax; int $0x80"
                 :
                 : "b"(&payload)
                 : "eax", "ecx", "edx", "memory");
}

static void sput(char c) {
    uint32_t _c = (uint32_t)(unsigned char)c;
    asm volatile("mov $6, %%eax; int $0x80" : : "b"(_c) : "eax", "ecx", "edx", "memory");
}

static volatile uint16_t *vga = (uint16_t *)0xB8000;
static void vga_put(char c) {
    static int pos = 0;
    if (pos < 80 * 25) {
        vga[pos] = (uint16_t)(uint8_t)c | (uint16_t)0x0F00;
        pos++;
    }
}

static void swrite(const char *s) {
    while (*s) {
        if (*s == '\n') sput('\r');
        vga_put(*s);
        sput(*s++);
        vga_put('.');
    }
}

void kernel_main(void) {
    uint32_t my_id = *(volatile uint32_t *)0x100FFB0;
    swrite("K");
    swrite("0");
    swrite("0");
    swrite("0");
    swrite("0");
    swrite("0");
    swrite("0");
    if (my_id == 1) { swrite("1"); } else { swrite("X"); }
    swrite(" alive\n");

    uint32_t iter = 0;
    while (1) {
        if ((iter++ % 8) == 0) {
            heartbeat();
        }
        for (volatile int i = 0; i < 5000000; i++);
    }
}
