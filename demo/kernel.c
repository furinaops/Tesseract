#include <stdint.h>
#include <stddef.h>
#include "mini_py.h"

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

#define VGA_MEMORY  ((uint16_t *)0xB8000)
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static uint8_t vga_color = 0x07;
static int cursor_x = 0;
static int cursor_y = 0;

static void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
    } else {
        VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)(uint8_t)c | (uint16_t)vga_color << 8;
        cursor_x++;
    }
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VGA_HEIGHT) {
        for (int y = 0; y < VGA_HEIGHT - 1; y++)
            for (int x = 0; x < VGA_WIDTH; x++)
                VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y + 1) * VGA_WIDTH + x];
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)0x0700;
        cursor_y = VGA_HEIGHT - 1;
    }
    outb(0x3F8, (uint8_t)c);
}

static void vga_write(const char *s) {
    for (; *s; s++) {
        if (*s == '\n') vga_putchar('\r');
        vga_putchar(*s);
    }
}

static int key_ready(void) {
    return inb(KEYBOARD_STATUS_PORT) & 1;
}

static int shift_pressed = 0;

static uint8_t read_key(void) {
    while (!key_ready());
    return inb(KEYBOARD_DATA_PORT);
}

static int handle_scancode(uint8_t sc) {
    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return -1; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; return -1; }
    if (sc >= 0x80) return -1;

    static const char unshifted[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
        0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
        '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
        0, ' ', 0
    };
    static const char shifted[] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
        0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,
        '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
        0, ' ', 0
    };
    if (sc < sizeof(unshifted))
        return shift_pressed ? shifted[sc] : unshifted[sc];
    return 0;
}

#define INPUT_BUF_SIZE 256
static char input_buf[INPUT_BUF_SIZE];
static int input_len = 0;

/* ─── Simple RAM Filesystem ─────────────────────────────── */

#define MAX_NODES 128
#define MAX_DATA (64 * 1024)

typedef enum { FT_FILE, FT_DIR } fs_type_t;

typedef struct fs_node {
    char name[64];
    fs_type_t type;
    int size;
    uint8_t *data;
    struct fs_node *parent;
    struct fs_node *children;
    struct fs_node *next;
} fs_node_t;

static fs_node_t fs_pool[MAX_NODES];
static int fs_pool_used = 0;
static uint8_t fs_data[MAX_DATA];
static int fs_data_used = 0;
static fs_node_t *fs_root = NULL;
static fs_node_t *fs_cwd = NULL;

static fs_node_t *fs_alloc_node(const char *name, fs_type_t type, fs_node_t *parent) {
    if (fs_pool_used >= MAX_NODES) return NULL;
    fs_node_t *n = &fs_pool[fs_pool_used++];
    int i = 0;
    while (name[i] && i < 63) { n->name[i] = name[i]; i++; }
    n->name[i] = '\0';
    n->type = type;
    n->size = 0;
    n->data = NULL;
    n->parent = parent;
    n->children = NULL;
    n->next = NULL;
    return n;
}

static uint8_t *fs_alloc_data(int size) {
    if (fs_data_used + size > MAX_DATA) return NULL;
    uint8_t *p = &fs_data[fs_data_used];
    fs_data_used += size;
    return p;
}

static void fs_init(void) {
    fs_root = fs_alloc_node("/", FT_DIR, NULL);
    fs_cwd = fs_root;
}

static fs_node_t *fs_find(fs_node_t *dir, const char *name) {
    for (fs_node_t *c = dir->children; c; c = c->next)
        if (strcmp(c->name, name) == 0) return c;
    return NULL;
}

static int fs_mkfile(fs_node_t *dir, const char *name) {
    if (fs_find(dir, name)) return -1;
    fs_node_t *n = fs_alloc_node(name, FT_FILE, dir);
    if (!n) return -1;
    n->next = dir->children;
    dir->children = n;
    return 0;
}

static int fs_mkdir(fs_node_t *dir, const char *name) {
    if (fs_find(dir, name)) return -1;
    fs_node_t *n = fs_alloc_node(name, FT_DIR, dir);
    if (!n) return -1;
    n->next = dir->children;
    dir->children = n;
    return 0;
}

static int fs_write(fs_node_t *file, const uint8_t *data, int len) {
    uint8_t *buf = fs_alloc_data(len);
    if (!buf) return -1;
    file->data = buf;
    file->size = len;
    for (int i = 0; i < len; i++) buf[i] = data[i];
    return 0;
}

static int fs_remove(fs_node_t *parent, const char *name) {
    fs_node_t **pp = &parent->children;
    while (*pp) {
        if (strcmp((*pp)->name, name) == 0) {
            *pp = (*pp)->next;
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

static void fs_path_of(fs_node_t *n, char *buf, int bufsz) {
    if (!n || !n->parent) { buf[0] = '/'; buf[1] = '\0'; return; }
    char stack[64][64];
    int sp = 0;
    for (fs_node_t *p = n; p && p->parent; p = p->parent) {
        int i = 0;
        while (p->name[i] && i < 63) { stack[sp][i] = p->name[i]; i++; }
        stack[sp][i] = '\0';
        sp++;
    }
    int pos = 0;
    for (int i = sp - 1; i >= 0; i--) {
        if (pos + 1 < bufsz) buf[pos++] = '/';
        for (int j = 0; stack[i][j] && pos + 1 < bufsz; j++)
            buf[pos++] = stack[i][j];
    }
    if (pos == 0 && bufsz > 0) buf[pos++] = '/';
    buf[pos] = '\0';
}

static fs_node_t *fs_resolve(const char *path) {
    if (strcmp(path, "/") == 0) return fs_root;
    if (strcmp(path, ".") == 0) return fs_cwd;
    if (strcmp(path, "..") == 0) return fs_cwd->parent ? fs_cwd->parent : fs_root;
    fs_node_t *dir = (path[0] == '/') ? fs_root : fs_cwd;
    const char *p = (path[0] == '/') ? path + 1 : path;
    while (*p) {
        char part[64];
        int i = 0;
        while (*p && *p != '/' && i < 63) part[i++] = *p++;
        part[i] = '\0';
        if (*p == '/') p++;
        if (strcmp(part, "..") == 0) { dir = dir->parent ? dir->parent : dir; continue; }
        if (strcmp(part, ".") == 0) continue;
        fs_node_t *child = fs_find(dir, part);
        if (!child) return NULL;
        dir = child;
    }
    return dir;
}

/* ─── Python ─────────────────────────────────────────────── */

static void py_output(char c) {
    vga_putchar(c);
}

/* ─── Shell ──────────────────────────────────────────────── */

static void shell_ls(int argc, char **argv) {
    (void)argc; (void)argv;
    for (fs_node_t *c = fs_cwd->children; c; c = c->next) {
        vga_write(c->name);
        if (c->type == FT_DIR) vga_write("/");
        vga_write("  ");
    }
}

static void shell_cat(int argc, char **argv) {
    if (argc < 2) { vga_write("usage: cat <file>"); return; }
    fs_node_t *n = fs_find(fs_cwd, argv[1]);
    if (!n || n->type != FT_FILE) { vga_write("not found"); return; }
    for (int i = 0; i < n->size; i++) vga_putchar((char)n->data[i]);
}

static void shell_touch(int argc, char **argv) {
    if (argc < 2) { vga_write("usage: touch <file>"); return; }
    if (fs_mkfile(fs_cwd, argv[1]) < 0) vga_write("error");
}

static void shell_echo(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[2], ">") != 0) {
        for (int i = 1; i < argc; i++) {
            vga_write(argv[i]);
            if (i + 1 < argc) vga_write(" ");
        }
        return;
    }
    fs_node_t *n = fs_find(fs_cwd, argv[3]);
    if (!n) {
        if (fs_mkfile(fs_cwd, argv[3]) < 0) { vga_write("error"); return; }
        n = fs_find(fs_cwd, argv[3]);
    }
    int len = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) break;
        for (int j = 0; argv[i][j]; j++) len++;
        if (i + 1 < argc && strcmp(argv[i+1], ">") != 0) len++;
    }
    uint8_t *buf = fs_alloc_data(len);
    if (!buf) { vga_write("out of memory"); return; }
    n->data = buf;
    n->size = len;
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) break;
        for (int j = 0; argv[i][j]; j++) buf[pos++] = (uint8_t)argv[i][j];
        if (i + 1 < argc && strcmp(argv[i+1], ">") != 0) buf[pos++] = ' ';
    }
}

static void shell_mkdir(int argc, char **argv) {
    if (argc < 2) { vga_write("usage: mkdir <dir>"); return; }
    if (fs_mkdir(fs_cwd, argv[1]) < 0) vga_write("error");
}

static void shell_cd(int argc, char **argv) {
    if (argc < 2) return;
    fs_node_t *n = fs_resolve(argv[1]);
    if (n && n->type == FT_DIR) fs_cwd = n;
}

static void shell_rm(int argc, char **argv) {
    if (argc < 2) { vga_write("usage: rm <file>"); return; }
    if (fs_remove(fs_cwd, argv[1]) < 0) vga_write("not found");
}

static void shell_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    cursor_x = 0; cursor_y = 0;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = (uint16_t)0x0700;
}

static void shell_help(int argc, char **argv) {
    (void)argc; (void)argv;
    vga_write("commands: ls, cat, touch, echo, mkdir, cd, rm, clear, py, help");
}

typedef struct { const char *name; void (*fn)(int, char **); } cmd_t;

/* ─── DOOM integration ──────────────────────────── */

extern volatile int g_doom_running;
extern void vga_set_mode_13h(void);
extern int printf(const char *fmt, ...);
extern void vga_restore_text_mode(void);
extern void doomgeneric_Create(int argc, char **argv);
extern void doomgeneric_Tick(void);

static void shell_doom(int argc, char **argv) {
    (void)argc; (void)argv;
    g_doom_running = 1;
    vga_set_mode_13h();
    static char *doom_argv[] = { "doom", "-iwad", "doom1.wad", "-skill", "1", "-warp", "1", "1", NULL };
    doomgeneric_Create(8, doom_argv);
    while (g_doom_running) {
        doomgeneric_Tick();
    }
    vga_restore_text_mode();
    while (inb(0x64) & 1) inb(0x60);
}

static cmd_t commands[] = {
    {"ls", shell_ls}, {"cat", shell_cat}, {"touch", shell_touch},
    {"echo", shell_echo}, {"mkdir", shell_mkdir}, {"cd", shell_cd},
    {"rm", shell_rm}, {"clear", shell_clear}, {"help", shell_help},
    {"doom", shell_doom},
    {NULL, NULL}
};

static void parse_and_run(const char *line) {
    /* special case: py command takes the raw remainder */
    if (line[0] == 'p' && line[1] == 'y' && (line[2] == ' ' || line[2] == '\0')) {
        mini_py_set_output(py_output);
        const char *src = line + 2;
        while (*src == ' ') src++;
        mini_py_exec(src);
        return;
    }

    char *argv[16];
    int argc = 0;
    char buf[INPUT_BUF_SIZE];
    int i = 0;
    while (*line && i < INPUT_BUF_SIZE - 1) {
        while (*line == ' ') line++;
        if (!*line) break;
        argv[argc++] = &buf[i];
        while (*line && *line != ' ' && i < INPUT_BUF_SIZE - 1) buf[i++] = *line++;
        buf[i++] = '\0';
        if (argc >= 16) break;
    }
    if (argc == 0) return;
    for (int c = 0; commands[c].name; c++) {
        if (strcmp(argv[0], commands[c].name) == 0) {
            commands[c].fn(argc, argv);
            return;
        }
    }
    vga_write("unknown command: ");
    vga_write(argv[0]);
}

/* ─── Input ──────────────────────────────────────────────── */

static void handle_enter(void) {
    vga_putchar('\r');
    vga_putchar('\n');
    input_buf[input_len] = '\0';
    parse_and_run(input_buf);
    input_len = 0;
    vga_putchar('\r');
    vga_putchar('\n');
    char path[256];
    fs_path_of(fs_cwd, path, sizeof(path));
    vga_write(path);
    vga_write(" *> ");
}

static void handle_backspace(void) {
    if (input_len > 0) {
        input_len--;
        if (cursor_x > 0) {
            cursor_x--;
            VGA_MEMORY[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)0x0700;
        }
        outb(0x3F8, '\b');
    }
}

static void handle_char(char c) {
    if (input_len < INPUT_BUF_SIZE - 1) {
        input_buf[input_len++] = c;
        vga_putchar(c);
    }
}

/* ─── Entry ──────────────────────────────────────────────── */

void kernel_main(void) {
    fs_init();
    fs_mkdir(fs_root, "home");
    fs_mkdir(fs_root, "etc");
    fs_mkdir(fs_root, "usr");
    fs_node_t *home = fs_find(fs_root, "home");
    if (home) fs_cwd = home;
    fs_mkfile(home, "hello.txt");
    fs_node_t *hi = fs_find(home, "hello.txt");
    if (hi) {
        const char *msg = "Welcome to Tesseract!\nThis is a simple RAM filesystem.\n";
        int len = 0;
        while (msg[len]) len++;
        fs_write(hi, (const uint8_t *)msg, len);
    }

    vga_write("Tesseract kernel booted...\r\n");
    vga_write("Hyperkernel... [checked]\r\n");
    vga_write("kernel 1 instance... [checked]\r\n");
    vga_write("filesystem... [checked]\r\n");
    vga_write("\r\nWelcome to Tesseract!\r\n");
    vga_write("\r\n");
    vga_write("/home *> ");

    while (1) {
        uint8_t sc = read_key();
        if (sc >= 0x80 && sc != 0xAA && sc != 0xB6) continue;

        if (sc == 0x1C) {
            handle_enter();
        } else if (sc == 0x0E) {
            handle_backspace();
        } else {
            int c = handle_scancode(sc);
            if (c > 0) handle_char((char)c);
        }
    }
}
