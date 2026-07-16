#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>
#define RAND_MAX 32767
void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);
void exit(int status);
int atoi(const char *str);
double atof(const char *str);
int rand(void);
void srand(unsigned int seed);
void abort(void);
int abs(int x);
#endif
