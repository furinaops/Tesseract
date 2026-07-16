#ifndef _DIRENT_H
#define _DIRENT_H
typedef struct dirent { char d_name[256]; } DIR;
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);
#endif
