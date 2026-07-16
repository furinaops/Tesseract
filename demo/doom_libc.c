#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <poll.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#define HEAP_SIZE (64 * 1024 * 1024)
static char g_heap[HEAP_SIZE];
static size_t g_heap_top;

void *malloc(size_t size) {
    size = (size + 7) & ~7;
    if (g_heap_top + size > HEAP_SIZE) return 0;
    void *p = g_heap + g_heap_top;
    g_heap_top += size;
    return p;
}

void free(void *p) { (void)p; }

void *realloc(void *p, size_t size) {
    if (!p) return malloc(size);
    return malloc(size);
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) { char *cp = p; for (size_t i = 0; i < total; i++) cp[i] = 0; }
    return p;
}

int abs(int x) { return x < 0 ? -x : x; }
void abort(void) { for (;;) asm volatile("cli; hlt"); }

void exit(int status) { (void)status; for (;;) asm volatile("cli; hlt"); }

/* ─── WAD file handling ──────────────────────── */

extern const unsigned char _binary_doom1_wad_start[];
extern const unsigned char _binary_doom1_wad_end[];
extern const int _binary_doom1_wad_size;

static struct wad_file {
    const unsigned char *data;
    int size;
    int pos;
    int open;
} g_wad;

static int wad_strcicmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 0x20;
        if (cb >= 'A' && cb <= 'Z') cb += 0x20;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

/* Custom file struct (matches our stdio.h definition) */
typedef struct _iobuf {
    int fd;
    int flags;
} FILE_IMPL;

FILE_IMPL __stdin  = { 0, 0 };
FILE_IMPL __stdout = { 0, 0 };
FILE_IMPL __stderr = { 0, 0 };

static int has_suffix(const char *s, const char *suf) {
    size_t sl = strlen(s), sul = strlen(suf);
    return sl >= sul && wad_strcicmp(s + sl - sul, suf) == 0;
}

FILE_IMPL *fopen(const char *path, const char *mode) {
    (void)mode;
    if (!g_wad.open && has_suffix(path, ".wad")) {
        g_wad.data = _binary_doom1_wad_start;
        g_wad.size = (int)(unsigned long)&_binary_doom1_wad_size;
        g_wad.pos = 0;
        g_wad.open = 1;
    }
    if (!g_wad.open) return 0;
    g_wad.pos = 0;
    static FILE_IMPL f = { 1, 0 };
    return &f;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE_IMPL *stream) {
    (void)ptr; (void)size; (void)nmemb; (void)stream;
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE_IMPL *stream) {
    (void)stream;
    size_t total = size * nmemb;
    size_t wsize = (size_t)g_wad.size;
    size_t avail = ((size_t)g_wad.pos + total > wsize) ? wsize - (size_t)g_wad.pos : total;
    const unsigned char *src = g_wad.data + g_wad.pos;
    unsigned char *dst = ptr;
    for (size_t i = 0; i < avail; i++) dst[i] = src[i];
    g_wad.pos += (int)avail;
    return avail / size;
}

int fclose(FILE_IMPL *stream) { (void)stream; return 0; }

int fseek(FILE_IMPL *stream, long offset, int whence) {
    (void)stream;
    if (whence == 0) g_wad.pos = (int)offset;
    else if (whence == 1) g_wad.pos += (int)offset;
    else if (whence == 2) g_wad.pos = g_wad.size + (int)offset;
    return 0;
}

long ftell(FILE_IMPL *stream) { (void)stream; return g_wad.pos; }

int fflush(FILE_IMPL *stream) { (void)stream; return 0; }

/* ─── printf ─────────────────────────────────── */

static char printf_buf[256];
static int printf_pos;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static void serial_putc(char c) {
    while (!(inb(0x3FD) & 0x20));
    outb(0x3F8, (uint8_t)(unsigned char)c);
}

static void printf_flush(void) {
    for (int i = 0; i < printf_pos; i++) serial_putc(printf_buf[i]);
    printf_pos = 0;
}

static void fmt_str(const char *s) {
    while (*s) {
        if (printf_pos >= (int)sizeof(printf_buf) - 1) printf_flush();
        printf_buf[printf_pos++] = *s++;
    }
}

static void fmt_int(int val, int base, int sign) {
    char buf[16]; int pos = 0;
    unsigned int uv;
    if (sign && val < 0) { fmt_str("-"); uv = (unsigned int)(-val); }
    else uv = (unsigned int)val;
    if (uv == 0) { buf[pos++] = '0'; }
    else while (uv > 0) { int d = uv % base; buf[pos++] = d < 10 ? '0'+d : 'a'+d-10; uv /= base; }
    while (pos > 0) {
        if (printf_pos >= (int)sizeof(printf_buf) - 1) printf_flush();
        printf_buf[printf_pos++] = buf[--pos];
    }
}

static int vsnprintf_parse_int(const char **fmt) {
    int val = 0;
    while (**fmt >= '0' && **fmt <= '9') { val = val * 10 + (**fmt - '0'); (*fmt)++; }
    return val;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    int written = 0;
    while (*fmt) {
        if (*fmt != '%') {
            if (written < (int)n - 1) buf[written] = *fmt;
            written++; fmt++; continue;
        }
        fmt++;
        int width = 0, precision = -1;
        if (*fmt == '0') { fmt++; }
        if (*fmt >= '1' && *fmt <= '9') { width = vsnprintf_parse_int(&fmt); }
        if (*fmt == '.') { fmt++; precision = vsnprintf_parse_int(&fmt); }
        (void)width;
        if (*fmt == 's') {
            const char *s = va_arg(ap, const char*);
            int printed = 0;
            if (!s) s = "(null)";
            while (*s && (precision < 0 || printed < precision)) {
                if (written < (int)n - 1) buf[written] = *s;
                written++; s++; printed++;
            }
        } else if (*fmt == 'd' || *fmt == 'i') {
            int val = va_arg(ap, int);
            char tmp[32]; int p = 0; unsigned int uv;
            if (val < 0) { if (written < (int)n - 1) buf[written++] = '-'; uv = (unsigned int)(-val); }
            else uv = (unsigned int)val;
            if (uv == 0) tmp[p++] = '0';
            while (uv > 0 && p < 30) { tmp[p++] = '0' + (uv % 10); uv /= 10; }
            if (precision > 0) {
                while (p < precision) tmp[p++] = '0';
            }
            while (p > 0) { if (written < (int)n - 1) buf[written++] = tmp[--p]; }
        } else if (*fmt == 'u') {
            unsigned int uv = va_arg(ap, unsigned int);
            char tmp[16]; int p = 0;
            if (uv == 0) tmp[p++] = '0';
            while (uv > 0) { tmp[p++] = '0' + (uv % 10); uv /= 10; }
            while (p > 0) { if (written < (int)n - 1) buf[written++] = tmp[--p]; }
        } else if (*fmt == 'x' || *fmt == 'X') {
            unsigned int uv = va_arg(ap, unsigned int);
            char tmp[16]; int p = 0;
            if (uv == 0) tmp[p++] = '0';
            while (uv > 0) { int d = uv % 16; tmp[p++] = d < 10 ? '0'+d : (*fmt=='X' ? 'A'+d-10 : 'a'+d-10); uv /= 16; }
            while (p > 0) { if (written < (int)n - 1) buf[written++] = tmp[--p]; }
        } else if (*fmt == 'c') {
            char c = (char)va_arg(ap, int);
            if (written < (int)n - 1) buf[written++] = c;
        } else if (*fmt == '%') {
            if (written < (int)n - 1) buf[written++] = '%';
        } else {
            return written;
        }
        fmt++;
    }
    if (written < (int)n) buf[written] = '\0';
    else if (n > 0) buf[n-1] = '\0';
    return written;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap); return r;
}

int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    printf_pos = 0;
    while (*fmt) {
        if (*fmt != '%') {
            if (printf_pos >= (int)sizeof(printf_buf) - 1) printf_flush();
            printf_buf[printf_pos++] = *fmt++; continue;
        }
        fmt++;
        if (*fmt == 's') fmt_str(va_arg(ap, const char*));
        else if (*fmt == 'd' || *fmt == 'i') fmt_int(va_arg(ap, int), 10, 1);
        else if (*fmt == 'u') fmt_int(va_arg(ap, unsigned int), 10, 0);
        else if (*fmt == 'x' || *fmt == 'X') fmt_int(va_arg(ap, unsigned int), 16, 0);
        else if (*fmt == 'c') { char c = (char)va_arg(ap, int); if (printf_pos >= (int)sizeof(printf_buf)-1) printf_flush(); printf_buf[printf_pos++] = c; }
        else if (*fmt == '%') { if (printf_pos >= (int)sizeof(printf_buf)-1) printf_flush(); printf_buf[printf_pos++] = '%'; }
        fmt++;
    }
    printf_flush();
    va_end(ap); return 0;
}

int fprintf(FILE_IMPL *f, const char *fmt, ...) {
    (void)f; va_list ap; va_start(ap, fmt);
    printf_pos = 0;
    while (*fmt) {
        if (*fmt != '%') { if (printf_pos >= (int)sizeof(printf_buf)-1) printf_flush(); printf_buf[printf_pos++] = *fmt++; continue; }
        fmt++;
        if (*fmt == 's') fmt_str(va_arg(ap, const char*));
        else if (*fmt == 'd' || *fmt == 'i') fmt_int(va_arg(ap, int), 10, 1);
        else fmt++;
        fmt++;
    }
    printf_flush();
    va_end(ap); return 0;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = vsnprintf(buf, 65536, fmt, ap);
    va_end(ap); return r;
}

int sscanf(const char *str, const char *fmt, ...) {
    (void)str; (void)fmt;
    return 0;
}

int putchar(int c) {
    serial_putc((char)c);
    return c;
}

int puts(const char *s) {
    while (*s) { serial_putc(*s++); }
    serial_putc('\n');
    return 0;
}

int vfprintf(FILE_IMPL *stream, const char *fmt, va_list ap) {
    (void)stream;
    printf_pos = 0;
    while (*fmt) {
        if (*fmt != '%') { if (printf_pos >= (int)sizeof(printf_buf)-1) printf_flush(); printf_buf[printf_pos++] = *fmt++; continue; }
        fmt++;
        while (*fmt == '.' || (*fmt >= '0' && *fmt <= '9')) fmt++;
        if (*fmt == 's') fmt_str(va_arg(ap, const char*));
        else if (*fmt == 'd' || *fmt == 'i') fmt_int(va_arg(ap, int), 10, 1);
        else if (*fmt == 'u') fmt_int(va_arg(ap, unsigned int), 10, 0);
        else if (*fmt == '%') { if (printf_pos >= (int)sizeof(printf_buf)-1) printf_flush(); printf_buf[printf_pos++] = '%'; }
        fmt++;
    }
    printf_flush();
    return 0;
}

int fputs(const char *s, FILE_IMPL *stream) { (void)stream; puts(s); return 0; }

int remove(const char *path) { (void)path; return -1; }
int rename(const char *old, const char *new) { (void)old; (void)new; return -1; }

void perror(const char *s) { printf("Error: %s\n", s); }

/* ─── ctype ──────────────────────────────────── */

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isprint(int c) { return c >= 0x20 && c <= 0x7E; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 0x20 : c; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 0x20 : c; }

/* ─── string ─────────────────────────────────── */

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d <= s) for (size_t i = 0; i < n; i++) d[i] = s[i];
    else for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = s1, *b = s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t n = 0; while (s[n]) n++; return n;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n && (*a || *b); i++) {
        if (*a != *b) return (unsigned char)*a - (unsigned char)*b;
        if (*a) a++;
        if (*b) b++;
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest; while (*src) *d++ = *src++; *d = '\0'; return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    for (size_t i = 0; i < n; i++) {
        if (*src) { *d++ = *src++; } else { *d++ = '\0'; }
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while (*src) *d++ = *src++;
    *d = '\0';
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return (char*)s; s++; }
    return 0;
}

char *strrchr(const char *s, int c) {
    const char *last = 0;
    while (*s) { if (*s == (char)c) last = s; s++; }
    return (char*)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char*)haystack;
    while (*haystack) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
        haystack++;
    }
    return 0;
}

int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 0x20;
        if (cb >= 'A' && cb <= 'Z') cb += 0x20;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncasecmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (!a[i] || !b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 0x20;
        if (cb >= 'A' && cb <= 'Z') cb += 0x20;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
    }
    return 0;
}

char *strdup(const char *s) {
    size_t len = strlen(s);
    char *p = malloc(len + 1);
    if (p) { for (size_t i = 0; i < len; i++) p[i] = s[i]; p[len] = '\0'; }
    return p;
}

/* ─── stdlib extras ──────────────────────────── */

int atoi(const char *s) {
    int val = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; } else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
    return sign * val;
}

double atof(const char *s) { (void)s; return 0.0; }

static unsigned int g_rand_seed = 1;
int rand(void) {
    g_rand_seed = g_rand_seed * 1103515245 + 12345;
    return (g_rand_seed / 65536) % 32768;
}
void srand(unsigned int seed) { g_rand_seed = seed; }

/* ─── setjmp/longjmp (i386) ──────────────────── */

int setjmp(jmp_buf env) {
    asm volatile("movl %%ebx, (%0)\n"
                 "movl %%esi, 4(%0)\n"
                 "movl %%edi, 8(%0)\n"
                 "movl %%ebp, 12(%0)\n"
                 "movl %%esp, 16(%0)\n"
                 "movl (%%esp), %%eax\n"
                 "movl %%eax, 20(%0)\n"
                 : : "r"(env) : "memory");
    return 0;
}

void longjmp(jmp_buf env, int val) {
    asm volatile("movl (%0), %%ebx\n"
                 "movl 4(%0), %%esi\n"
                 "movl 8(%0), %%edi\n"
                 "movl 12(%0), %%ebp\n"
                 "movl 16(%0), %%esp\n"
                 "movl %1, %%eax\n"
                 "jmp *20(%0)\n"
                 : : "r"(env), "r"(val) : "memory");
}

/* ─── math stubs ──────────────────────────────── */

double sin(double x) { (void)x; return 0.0; }
double cos(double x) { (void)x; return 1.0; }
double sqrt(double x) { (void)x; return 0.0; }
double fabs(double x) { (void)x; return x; }

/* Soft-float helpers needed by -mno-80387 code */
float __subsf3(float a, float b) { return a - b; }
float __addsf3(float a, float b) { return a + b; }
float __mulsf3(float a, float b) { return a * b; }
float __divsf3(float a, float b) { return a / b; }
float __negsf2(float a) { return -a; }
int __fixsfsi(float x) { return (int)x; }
unsigned int __fixunssfsi(float x) { return (unsigned int)x; }
float __floatsisf(int i) { return (float)i; }
float __floatunsisf(unsigned int i) { return (float)i; }
int __cmpsf2(float a, float b) { return a < b ? -1 : a > b ? 1 : 0; }
int __eqsf2(float a, float b) { return a == b ? 0 : 1; }
int __nesf2(float a, float b) { return a != b ? 0 : 1; }
int __ltsf2(float a, float b) { return a < b ? -1 : a >= b ? 0 : 1; }
int __lesf2(float a, float b) { return a <= b ? -1 : a > b ? 0 : 1; }
int __gtsf2(float a, float b) { return a > b ? 1 : a <= b ? 0 : -1; }
int __gesf2(float a, float b) { return a >= b ? 0 : -1; }

double __extendsfdf2(float f) { return (double)f; }
float __truncdfsf2(double d) { return (float)d; }
double __adddf3(double a, double b) { return a + b; }
double __subdf3(double a, double b) { return a - b; }
double __muldf3(double a, double b) { return a * b; }
double __divdf3(double a, double b) { return a / b; }
double __negdf2(double a) { return -a; }
int __fixdfsi(double x) { return (int)x; }
unsigned int __fixunsdfsi(double x) { return (unsigned int)x; }
double __floatsidf(int i) { return (double)i; }
double __floatunsidf(unsigned int i) { return (double)i; }
int __cmpdf2(double a, double b) { return a < b ? -1 : a > b ? 1 : 0; }
int __eqdf2(double a, double b) { return a == b ? 0 : 1; }
int __nedf2(double a, double b) { return a != b ? 0 : 1; }
int __ltdf2(double a, double b) { return a < b ? -1 : a >= b ? 0 : 1; }
int __ledf2(double a, double b) { return a <= b ? -1 : a > b ? 0 : 1; }
int __gtdf2(double a, double b) { return a > b ? 1 : a <= b ? 0 : -1; }
int __gedf2(double a, double b) { return a >= b ? 0 : -1; }

/* unistd */
int isatty(int fd) { (void)fd; return 0; }
int fileno(FILE_IMPL *stream) { (void)stream; return 0; }
int system(const char *cmd) { (void)cmd; return -1; }

int gettimeofday(struct timeval *tv, void *tz) { (void)tv; (void)tz; return 0; }

/* sys/stat */
int stat(const char *path, struct stat *buf) { (void)path; buf->st_size = 0; return 0; }
int fstat(int fd, struct stat *buf) { (void)fd; buf->st_size = 0; return 0; }
int mkdir(const char *path, mode_t mode) { (void)path; (void)mode; return -1; }

/* sys/mman */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset) { (void)addr; (void)length; (void)prot; (void)flags; (void)fd; (void)offset; return (void*)-1; }
int munmap(void *addr, size_t length) { (void)addr; (void)length; return 0; }

/* poll */
int poll(struct pollfd *fds, unsigned int nfds, int timeout) { (void)fds; (void)nfds; (void)timeout; return -1; }

/* dirent */
DIR *opendir(const char *name) { (void)name; return 0; }
struct dirent *readdir(DIR *dir) { (void)dir; return 0; }
int closedir(DIR *dir) { (void)dir; return 0; }

/* errno */
int errno = 0;
