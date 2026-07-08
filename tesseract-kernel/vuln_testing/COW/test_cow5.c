#include <stdint.h>

static void sput(char c) {
    asm volatile("int $0x80" : : "a"(6), "b"((uint32_t)c) : "ecx", "edx", "memory");
}
static void swrite(const char *str) {
    for (; *str; str++) { if (*str == '\n') sput('\r'); sput(*str); }
}
static void swritehex(uint32_t n) {
    for (int i = 7; i >= 0; i--) {
        char nib = (n >> (i * 4)) & 0xF;
        sput(nib < 10 ? '0' + nib : 'A' + nib - 10);
    }
}

void kernel_main(void) {
    uint32_t my_id = *(volatile uint32_t *)0x100FFB0;
    swrite("COW5 "); swritehex(my_id); swrite(" start\n");
    swrite("COW5 attempting write to g_state (0x108fa0)...\n");
    volatile uint32_t *target = (volatile uint32_t *)0x108fa0;
    *target = 0xBBBBBBBB;
    swrite("COW5 write SUCCEEDED (g_state writable!)\n");
    for (;;) { for (volatile int _i = 0; _i < 5000000; _i++); }
}
