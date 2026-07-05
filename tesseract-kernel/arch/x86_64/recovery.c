#include <stdint.h>

#define VGA_ADDR    ((uint16_t *)0xB8000)
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

static void rec_putchar(char c, int row, int col, uint8_t color) {
    VGA_ADDR[row * VGA_WIDTH + col] = (uint16_t)c | (uint16_t)color << 8;
}

static void rec_writestring(const char *str, int row, int col, uint8_t color) {
    int i = 0;
    while (str[i]) {
        rec_putchar(str[i], row, col + i, color);
        i++;
    }
}

void recovery_kernel_entry(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_ADDR[y * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)0x1F << 8;
        }
    }

    uint8_t red_on_white = 0x1F;
    rec_writestring(" EMERGENCY RECOVERY MODE ", 10, 22, red_on_white);
    rec_writestring(" All kernel instances terminated. ", 12, 18, 0x07);
    rec_writestring(" System halted. ", 14, 26, 0x07);

    asm volatile("cli");
    for (;;) {
        asm volatile("hlt");
    }
}
