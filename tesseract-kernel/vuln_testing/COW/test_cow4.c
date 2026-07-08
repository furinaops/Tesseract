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
    swrite("COW4 "); swritehex(my_id); swrite(" start\n");
    swrite("COW4 attempting write to code page (0x108000)...\n");
    volatile uint32_t *target = (volatile uint32_t *)0x108000;
    uint32_t old = *target;
    *target = 0xAAAAAAAA;
    uint32_t result = *target;
    if (result == 0xAAAAAAAA) {
        swrite("COW4 write SUCCEEDED (code page writable!)\n");
        *target = old;
    } else {
        swrite("COW4 write FAILED (code page not writable)\n");
    }
    for (;;) { for (volatile int _i = 0; _i < 5000000; _i++); }
}
