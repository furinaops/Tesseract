#ifndef _UNISTD_H
#define _UNISTD_H
int isatty(int fd);
int fileno(FILE *stream);
int system(const char *command);
#endif
