#ifndef DISPLAY_H
#define DISPLAY_H

#include <sys/stat.h>

/* Level 3: print size, permissions, owner, inode, mtime ... */
void print_file_info(const char *label, const char *path, const struct stat *st);

/* Level 4: progress bar (single-threaded use) */
void progress_start(void);
void progress_update(unsigned long long done, unsigned long long total);
void progress_finish(void);

#endif
