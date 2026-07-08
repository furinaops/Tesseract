#include "hyperkernel.h"

static uint16_t *g_terminal_buffer;
static int g_terminal_row;
static int g_terminal_col;
static uint8_t g_terminal_color;

enum vga_color {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_LIGHT_BROWN   = 14,
    VGA_WHITE         = 15,
};

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ADDR   ((uint16_t *)0xB8000)

static uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | (uint16_t)color << 8;
}

void terminal_initialize(void) {
    g_terminal_row = 0;
    g_terminal_col = 0;
    g_terminal_color = vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK);
    g_terminal_buffer = VGA_ADDR;
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const size_t idx = y * VGA_WIDTH + x;
            g_terminal_buffer[idx] = vga_entry(' ', g_terminal_color);
        }
    }
}

void terminal_setcolor(uint8_t color) {
    g_terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t idx = y * VGA_WIDTH + x;
    g_terminal_buffer[idx] = vga_entry((unsigned char)c, color);
}

static void terminal_scroll(void) {
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            g_terminal_buffer[y * VGA_WIDTH + x] =
                g_terminal_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        g_terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', g_terminal_color);
    }
}

void terminal_putchar(char c) {
    if (c == '\n') {
        g_terminal_col = 0;
        g_terminal_row++;
        if (g_terminal_row >= VGA_HEIGHT) {
            g_terminal_row = VGA_HEIGHT - 1;
            terminal_scroll();
        }
        return;
    }
    if (c == '\r') {
        g_terminal_col = 0;
        return;
    }
    if (c == '\t') {
        g_terminal_col = (g_terminal_col + 8) & ~7;
        return;
    }
    terminal_putentryat(c, g_terminal_color, g_terminal_col, g_terminal_row);
    g_terminal_col++;
    if (g_terminal_col >= VGA_WIDTH) {
        g_terminal_col = 0;
        g_terminal_row++;
        if (g_terminal_row >= VGA_HEIGHT) {
            g_terminal_row = VGA_HEIGHT - 1;
            terminal_scroll();
        }
    }
}

void terminal_writestring(const char *str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

void terminal_writehex(uint32_t n) {
    terminal_writestring("0x");
    for (int i = 7; i >= 0; i--) {
        char nibble = (n >> (i * 4)) & 0xF;
        terminal_putchar(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}

void terminal_writedec(uint32_t n) {
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    if (n == 0) {
        terminal_putchar('0');
        return;
    }
    while (n > 0 && i > 0) {
        i--;
        buf[i] = '0' + (n % 10);
        n /= 10;
    }
    terminal_writestring(&buf[i]);
}

hyperkernel_state_t g_state;

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
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') serial_putchar('\r');
        serial_putchar(str[i]);
    }
}

static void serial_writedec(uint32_t n) {
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    if (n == 0) {
        serial_putchar('0');
        return;
    }
    while (n > 0 && i > 0) {
        i--;
        buf[i] = '0' + (n % 10);
        n /= 10;
    }
    serial_writestring(&buf[i]);
}

extern volatile uint32_t g_hk_stack_canary;
#define STACK_CANARY_VALUE 0xDEADBEEF

void kernel_main(void) {
    g_hk_stack_canary = STACK_CANARY_VALUE;
    terminal_initialize();
    terminal_setcolor(vga_entry_color(VGA_CYAN, VGA_BLACK));
    terminal_writestring("Tesseract Hyperkernel v");
    terminal_writestring(HYPERKERNEL_VERSION);
    terminal_writestring("\n");
    serial_writestring("Tesseract Hyperkernel v");
    serial_writestring(HYPERKERNEL_VERSION);
    serial_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_LIGHT_GREY, VGA_BLACK));
    terminal_writestring("One Hyperkernel to rule them all.\n");
    serial_writestring("One Hyperkernel to rule them all.\n");
    terminal_writestring("Initializing memory... ");
    serial_writestring("Initializing memory... ");
    memory_init();
    terminal_writestring("OK\n");
    serial_writestring("OK\n");

    terminal_writestring("Initializing GDT... ");
    serial_writestring("Initializing GDT... ");
    hypervisor_init_gdt();
    terminal_writestring("OK\n");
    serial_writestring("OK\n");

    terminal_writestring("Initializing IDT... ");
    serial_writestring("Initializing IDT... ");
    interrupt_init();
    serial_writestring("OK\n");

    terminal_writestring("Initializing paging... ");
    serial_writestring("Initializing paging... ");
    paging_init();
    terminal_writestring("Enabling paging... ");
    serial_writestring("Enabling paging... ");
    paging_enable();
    serial_writestring("OK\n");

    terminal_writestring("Initializing PIT timer... ");
    serial_writestring("Initializing PIT timer... ");
    timer_init();
    serial_writestring("OK\n");

    terminal_writestring("Patching CR3 values... ");
    serial_writestring("Patching CR3 values... ");
    patch_cr3_values();
    serial_writestring("OK\n");

    priority_t priorities[] = {
        PRIORITY_CRITICAL,
        PRIORITY_HIGH,
        PRIORITY_NORMAL,
        PRIORITY_IDLE
    };
    uint32_t user_ids[] = { 1, 2, 3, 4 };

    int kids[4];
    for (int i = 0; i < 4; i++) {
        terminal_writestring("Spawning kernel instance ");
        terminal_writedec((uint32_t)(i + 1));
        terminal_writestring("... ");
        serial_writestring("Spawning kernel instance ");
        serial_writedec((uint32_t)(i + 1));
        serial_writestring("... ");
        int kid = spawn_instance(0, priorities[i], user_ids[i]);
        if (kid < 0) {
            terminal_writestring("FAILED\n");
            serial_writestring("FAILED\n");
            return;
        }
        kids[i] = kid;
        terminal_writestring("OK (id=");
        terminal_writedec((uint32_t)kid);
        terminal_writestring(")\n");
        serial_writestring("OK (id=");
        serial_writedec((uint32_t)kid);
        serial_writestring(")\n");
    }

    g_state.total_memory = MEMORY_POOL_SIZE;
    g_state.load_factor = 0;
    g_state.emergency_mode = 0;

    terminal_writestring("Enabling interrupts... ");
    serial_writestring("Enabling interrupts... ");
    asm volatile("sti");
    terminal_writestring("OK\n");
    serial_writestring("OK\n");

    terminal_writestring("Jumping to kernel instance...\n\n");
    serial_writestring("Jumping to kernel instance...\n\n");
    jump_to_kernel((uint32_t)kids[0]);
}
