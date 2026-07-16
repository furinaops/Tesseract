#include <stdint.h>
#include <stddef.h>
#include "doom/doomgeneric.h"
#include "doom/doomkeys.h"
#include "doom/doomtype.h"
#include "doom/d_mode.h"

#define VGA_FB ((volatile uint8_t*)0xA0000)
#define VGA_TEXT ((volatile uint16_t*)0xB8000)

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ─── VGA mode switching ─────────────────────── */

static void vga_out_seq(uint8_t idx, uint8_t val) {
    outb(0x3C4, idx); outb(0x3C5, val);
}
static void vga_out_crtc(uint8_t idx, uint8_t val) {
    outb(0x3D4, idx); outb(0x3D5, val);
}
static void vga_out_gfx(uint8_t idx, uint8_t val) {
    outb(0x3CE, idx); outb(0x3CF, val);
}
static void vga_out_attr(uint8_t idx, uint8_t val) {
    inb(0x3DA);
    outb(0x3C0, idx);
    outb(0x3C0, val);
}

void vga_set_mode_13h(void) {
    outb(0x3C2, 0x63);
    vga_out_seq(0x00, 0x03); vga_out_seq(0x01, 0x01);
    vga_out_seq(0x02, 0x0F); vga_out_seq(0x03, 0x00); vga_out_seq(0x04, 0x0E);
    vga_out_crtc(0x00, 0x5F); vga_out_crtc(0x01, 0x4F); vga_out_crtc(0x02, 0x50);
    vga_out_crtc(0x03, 0x82); vga_out_crtc(0x04, 0x54); vga_out_crtc(0x05, 0x80);
    vga_out_crtc(0x06, 0xBF); vga_out_crtc(0x07, 0x1F); vga_out_crtc(0x08, 0x00);
    vga_out_crtc(0x09, 0x41); vga_out_crtc(0x10, 0x9C); vga_out_crtc(0x11, 0x0E);
    vga_out_crtc(0x12, 0x8F); vga_out_crtc(0x13, 0x28); vga_out_crtc(0x14, 0x40);
    vga_out_crtc(0x15, 0x96); vga_out_crtc(0x16, 0xB9); vga_out_crtc(0x17, 0xA3);
    vga_out_crtc(0x18, 0xFF);
    vga_out_gfx(0x00, 0x00); vga_out_gfx(0x01, 0x00); vga_out_gfx(0x02, 0x00);
    vga_out_gfx(0x03, 0x00); vga_out_gfx(0x04, 0x00); vga_out_gfx(0x05, 0x40);
    vga_out_gfx(0x06, 0x05); vga_out_gfx(0x07, 0x0F); vga_out_gfx(0x08, 0xFF);
    vga_out_attr(0x10, 0x41); vga_out_attr(0x12, 0x0F); vga_out_attr(0x13, 0x08);
    vga_out_attr(0x14, 0x00); vga_out_attr(0x11, 0x00);
    outb(0x3C0, 0x20);
}

void vga_restore_text_mode(void) {
    vga_out_seq(0x00, 0x03); vga_out_seq(0x01, 0x01);
    vga_out_seq(0x02, 0x0F); vga_out_seq(0x03, 0x00); vga_out_seq(0x04, 0x03);
    vga_out_crtc(0x00, 0x5F); vga_out_crtc(0x01, 0x4F); vga_out_crtc(0x02, 0x50);
    vga_out_crtc(0x03, 0x82); vga_out_crtc(0x04, 0x55); vga_out_crtc(0x05, 0x81);
    vga_out_crtc(0x06, 0xBF); vga_out_crtc(0x07, 0x1F); vga_out_crtc(0x08, 0x00);
    vga_out_crtc(0x09, 0x4F); vga_out_crtc(0x10, 0x9C); vga_out_crtc(0x11, 0x0E);
    vga_out_crtc(0x12, 0x8F); vga_out_crtc(0x13, 0x28); vga_out_crtc(0x14, 0x1F);
    vga_out_crtc(0x15, 0x96); vga_out_crtc(0x16, 0xB9); vga_out_crtc(0x17, 0xA3);
    vga_out_crtc(0x18, 0xFF);
    vga_out_gfx(0x00, 0x00); vga_out_gfx(0x01, 0x00); vga_out_gfx(0x02, 0x00);
    vga_out_gfx(0x03, 0x00); vga_out_gfx(0x04, 0x00); vga_out_gfx(0x05, 0x10);
    vga_out_gfx(0x06, 0x0E); vga_out_gfx(0x07, 0x0F); vga_out_gfx(0x08, 0xFF);
    vga_out_attr(0x10, 0x0C); vga_out_attr(0x11, 0x00);
    outb(0x3C0, 0x20);
}

static void vga_set_palette(int idx, int r, int g, int b) {
    outb(0x3C8, (uint8_t)idx);
    outb(0x3C9, (uint8_t)(r >> 2));
    outb(0x3C9, (uint8_t)(g >> 2));
    outb(0x3C9, (uint8_t)(b >> 2));
}

/* ─── Keyboard scancode → DOOM key code ──────── */

static const unsigned char sc_to_doom[] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=',0x7f,9,
    'q','w','e','r','t','y','u','i','o','p','[',']',13,0xa3,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0xb6,
    '\\','z','x','c','v','b','n','m',',','.','/',0,0xb6,0,
    0xb8,' ',0xba,
    0xbb,0xbc,0xbd,0xbe,0xbf,0xc0,0xc1,0xc2,0xc3,0xc4,
    0xc5,0xc6,
};
#define SC_DOOM_SIZE (sizeof(sc_to_doom) / sizeof(sc_to_doom[0]))

static const unsigned char sc_ext_to_doom[] = {
    0x47,0xc7, 0x48,0xad, 0x49,0xc9,
    0x4b,0xac, 0x4d,0xae,
    0x4f,0xcf, 0x50,0xaf, 0x51,0xd1,
    0x52,0xd2, 0x53,0xd3,
};

static unsigned char map_scancode(unsigned char sc) {
    if (sc < SC_DOOM_SIZE) return sc_to_doom[sc];
    return 0;
}

static unsigned char map_ext_scancode(unsigned char sc) {
    for (int i = 0; i < (int)(sizeof(sc_ext_to_doom)/2); i++) {
        if (sc_ext_to_doom[i*2] == sc) return sc_ext_to_doom[i*2+1];
    }
    return 0;
}

/* ─── DG_* implementation ────────────────────── */

static int g_doom_ext_key = 0;

volatile int g_doom_running = 1;

/* Stubs for excluded platform files */
void I_Endoom(void) { }
void I_BindJoystickVariables(void) { }
void I_InitJoystick(void) { }
void StatDump(int p) { (void)p; }
void StatCopy(int p) { (void)p; }
unsigned int W_Checksum(unsigned char *buf, unsigned int len) { (void)buf; return len; }

extern int palette_changed;
typedef struct { unsigned char a, r, g, b; } color_t;
extern color_t colors[256];

void DG_Init(void) {
    vga_set_mode_13h();
    // Set a basic grayscale palette so initial rendering is visible
    for (int i = 0; i < 256; i++)
        vga_set_palette(i, i, i, i);
    g_doom_ext_key = 0;
}

void DG_DrawFrame(void) {
    volatile uint8_t *fb = VGA_FB;
    pixel_t *buf = DG_ScreenBuffer;
    for (int i = 0; i < DOOMGENERIC_RESX * DOOMGENERIC_RESY; i++)
        fb[i] = (uint8_t)buf[i];
    if (palette_changed) {
        for (int i = 0; i < 256; i++)
            vga_set_palette(i, colors[i].r, colors[i].g, colors[i].b);
        palette_changed = 0;
    }
}

void DG_SleepMs(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 5000; i++)
        asm volatile("pause");
}

uint32_t DG_GetTicksMs(void) {
    static uint32_t tick_counter = 0;
    return tick_counter++;
}

int DG_GetKey(int *pressed, unsigned char *key) {
    if (!(inb(0x64) & 1)) return 0;
    uint8_t sc = inb(0x60);

    if (sc == 0xE0) {
        g_doom_ext_key = 1;
        return DG_GetKey(pressed, key);
    }

    *pressed = !(sc & 0x80);
    sc &= 0x7F;

    if (g_doom_ext_key) {
        *key = map_ext_scancode(sc);
        g_doom_ext_key = 0;
    } else {
        *key = map_scancode(sc);
    }
    return *key != 0 || !*pressed;
}

void DG_SetWindowTitle(const char *title) { (void)title; }
