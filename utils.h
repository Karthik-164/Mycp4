#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <sys/types.h>

/* Write exactly 'count' bytes (handles partial writes and EINTR). 0 / -1 */
int write_all(int fd, const void *buf, size_t count);

/* Read until 'count' bytes or EOF (handles partial reads and EINTR). */
ssize_t read_full(int fd, void *buf, size_t count);

/* Parse "4096", "64K", "1M" into bytes. Returns 0 / -1. */
int parse_size(const char *text, size_t *out);

/* "1.50 MB" style text */
void format_size(unsigned long long bytes, char *out, size_t cap);

/* Monotonic clock in seconds */
double now_seconds(void);

/* out = dir + "/" + name. Returns -1 if it does not fit. */
int join_path(char *out, size_t cap, const char *dir, const char *name);

/* Last component of a path */
const char *base_name(const char *path);

#endif
