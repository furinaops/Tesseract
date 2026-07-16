#ifndef _SYS_STAT_H
#define _SYS_STAT_H
typedef unsigned int mode_t;
struct stat { int st_size; };
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int mkdir(const char *path, mode_t mode);
#endif
