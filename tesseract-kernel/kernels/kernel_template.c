#include <stdint.h>

#define VGA_ADDR    ((unsigned short *)0xB8000)
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

#define VGA_COLOR(fg, bg) ((fg) | (bg) << 4)

static unsigned short *buffer = VGA_ADDR;

static void putchar(char c, int row, int col, unsigned char color) {
    buffer[row * VGA_WIDTH + col] = (unsigned short)c | (unsigned short)color << 8;
}

static void write_string(const char *str, int row, int col, unsigned char color) {
    int i = 0;
    while (str[i]) {
        putchar(str[i], row, col + i, color);
        i++;
    }
}

static void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_putchar(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, (uint8_t)c);
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

static void heartbeat(void) {
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(5) : "memory");
    (void)ret;
}

static uint32_t get_id(void) {
    return *(volatile uint32_t *)0x100FFC0;
}

void kernel_main(void) {
    uint32_t my_id = get_id();
    uint8_t colors[] = { 2, 4, 1, 14 };
    uint8_t color = colors[(my_id - 1) % 4];
    int row = (my_id - 1) * 6;
    char msg[] = "Hello from Kernel Instance X!";
    msg[27] = '0' + (char)my_id;
    write_string(msg, row, 0, VGA_COLOR(color, 0));

    serial_writestring("K");
    serial_writehex(my_id);
    serial_writestring(" alive\n");

    uint32_t iter = 0;
    while (1) {
        if ((iter++ % 64) == 0) {
            heartbeat();
            serial_putchar('0' + (char)my_id);
        }
        asm volatile("hlt");
    }
}
