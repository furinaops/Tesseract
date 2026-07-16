#ifndef _STDIO_H
#define _STDIO_H
#include <stddef.h>
#include <stdarg.h>
#define NULL ((void*)0)
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
typedef struct _iobuf {
    int fd;
    int flags;
} FILE;
extern FILE __stdin;
extern FILE __stdout;
extern FILE __stderr;
#define stdin (&__stdin)
#define stdout (&__stdout)
#define stderr (&__stderr)
int printf(const char *fmt, ...);
int fprintf(FILE *f, const char *fmt, ...);
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, size_t n, const char *fmt, ...);
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int sscanf(const char *str, const char *fmt, ...);
int putchar(int c);
int puts(const char *s);
FILE *fopen(const char *path, const char *mode);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fclose(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int fflush(FILE *stream);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int fputs(const char *s, FILE *stream);
void perror(const char *s);
int remove(const char *path);
int rename(const char *old, const char *new);
#endif
