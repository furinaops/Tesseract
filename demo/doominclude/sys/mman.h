#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H
#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define MAP_PRIVATE 2
#define MAP_FAILED ((void*)-1)
void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset);
int munmap(void *addr, size_t length);
#endif
