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
    swrite("COW2 "); swritehex(my_id); swrite(" start\n");
    swrite("COW2 attempting write to PDE 2 range (0x800000)...\n");
    volatile uint32_t *target = (volatile uint32_t *)0x800000;
    *target = 0xCAFEBABE;
    swrite("COW2 write SUCCEEDED (PDE 2 is writable!)\n");
    for (;;) { for (volatile int _i = 0; _i < 5000000; _i++); }
}
