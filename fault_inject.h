/* fault_inject.h - force-included (-include) into the C sources under test.
 * Redirects selected libc calls to t_* wrappers (tests/fault.c) so the tests
 * can make them fail on demand.  Only the code under test is affected. */
#ifndef FAULT_INJECT_H
#define FAULT_INJECT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <stdarg.h>
#include <pthread.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

void           *t_malloc(size_t n);
char           *t_strdup(const char *s);
int             t_open(const char *path, int flags, ...);
ssize_t         t_read(int fd, void *buf, size_t n);
ssize_t         t_write(int fd, const void *buf, size_t n);
int             t_close(int fd);
off_t           t_lseek(int fd, off_t off, int whence);
int             t_fstat(int fd, struct stat *st);
int             t_stat(const char *path, struct stat *st);
int             t_lstat(const char *path, struct stat *st);
int             t_mkdir(const char *path, mode_t mode);
DIR            *t_opendir(const char *path);
int             t_chmod(const char *path, mode_t mode);
int             t_fchmod(int fd, mode_t mode);
int             t_pthread_create(pthread_t *t, const pthread_attr_t *a,
                                 void *(*fn)(void *), void *arg);
int             t_snprintf(char *buf, size_t cap, const char *fmt, ...)
                    __attribute__((format(printf, 3, 4)));
int             t_clock_gettime(clockid_t id, struct timespec *ts);
struct passwd  *t_getpwuid(uid_t uid);

#define malloc(n)              t_malloc(n)
#define strdup(s)              t_strdup(s)
#define open(...)              t_open(__VA_ARGS__)
#define read(fd, b, n)         t_read(fd, b, n)
#define write(fd, b, n)        t_write(fd, b, n)
#define close(fd)              t_close(fd)
#define lseek(fd, o, w)        t_lseek(fd, o, w)
#define fstat(fd, s)           t_fstat(fd, s)
#define stat(p, s)             t_stat(p, s)
#define lstat(p, s)            t_lstat(p, s)
#define mkdir(p, m)            t_mkdir(p, m)
#define opendir(p)             t_opendir(p)
#define chmod(p, m)            t_chmod(p, m)
#define fchmod(fd, m)          t_fchmod(fd, m)
#define pthread_create(t, a, f, x) t_pthread_create(t, a, f, x)
#define snprintf(...)          t_snprintf(__VA_ARGS__)
#define clock_gettime(i, t)    t_clock_gettime(i, t)
#define getpwuid(u)            t_getpwuid(u)

#endif
