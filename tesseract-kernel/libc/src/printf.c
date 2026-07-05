#include <stdarg.h>

extern void terminal_putchar(char c);
extern void terminal_writestring(const char *s);
extern void terminal_writehex(unsigned int n);
extern void terminal_writedec(unsigned int n);

static void print_int(int n) {
    if (n < 0) {
        terminal_putchar('-');
        n = -n;
    }
    unsigned int u = (unsigned int)n;
    char buf[12];
    int i = 11;
    buf[11] = '\0';
    if (u == 0) {
        terminal_putchar('0');
        return;
    }
    while (u > 0 && i > 0) {
        i--;
        buf[i] = '0' + (u % 10);
        u /= 10;
    }
    terminal_writestring(&buf[i]);
}

int printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = 0;

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            terminal_putchar(*p);
            count++;
            continue;
        }
        p++;
        switch (*p) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            terminal_writestring(s);
            count += strlen(s);
            break;
        }
        case 'd': {
            int n = va_arg(args, int);
            print_int(n);
            count++;
            break;
        }
        case 'x': {
            unsigned int n = va_arg(args, unsigned int);
            terminal_writehex(n);
            count += 8;
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            terminal_putchar(c);
            count++;
            break;
        }
        case '%':
            terminal_putchar('%');
            count++;
            break;
        default:
            terminal_putchar('%');
            terminal_putchar(*p);
            count += 2;
            break;
        }
    }

    va_end(args);
    return count;
}
