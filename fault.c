/* fault.c - implementation of the fault-injection layer (test only).
 * Compiled WITHOUT fault_inject.h, so the calls below reach the real libc. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "fault_ctl.h"

typedef struct {
    int calls;      /* calls seen              */
    int fail_at;    /* 1-based call to fail    */
    int sticky;     /* fail every later call too */
    int err;        /* errno to report         */
} slot_t;

static slot_t          g_slot[FI_COUNT];
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static size_t          g_read_max, g_write_max;
static int             g_trunc_len;
static double          g_clock[64];
static int             g_clock_n, g_clock_i;
static int             g_flip_at;

void fi_reset(void)
{
    pthread_mutex_lock(&g_mu);
    memset(g_slot, 0, sizeof(g_slot));
    g_read_max = g_write_max = 0;
    g_trunc_len = 0;
    g_clock_n = g_clock_i = 0;
    g_flip_at = 0;
    pthread_mutex_unlock(&g_mu);
}

void fi_fail_at(fi_fn_t fn, int nth, int err)
{
    g_slot[fn].fail_at = nth;
    g_slot[fn].sticky = 0;
    g_slot[fn].err = err ? err : EIO;
}

void fi_fail_from(fi_fn_t fn, int nth, int err)
{
    fi_fail_at(fn, nth, err);
    g_slot[fn].sticky = 1;
}

int fi_calls(fi_fn_t fn)
{
    return g_slot[fn].calls;
}

void fi_io_chunk(size_t read_max, size_t write_max)
{
    g_read_max = read_max;
    g_write_max = write_max;
}

void fi_snprintf_truncate(int reported_len)
{
    g_trunc_len = reported_len;
}

void fi_clock_script(const double *vals, int n)
{
    pthread_mutex_lock(&g_mu);
    g_clock_n = n > 64 ? 64 : n;
    memcpy(g_clock, vals, (size_t)g_clock_n * sizeof(double));
    g_clock_i = 0;
    pthread_mutex_unlock(&g_mu);
}

void fi_lstat_flip_dev_at(int nth)
{
    g_flip_at = nth;
}

/* Count the call; return the errno to inject, or 0 to let it through. */
static int hit(fi_fn_t fn)
{
    int err = 0;

    pthread_mutex_lock(&g_mu);
    g_slot[fn].calls++;
    if (g_slot[fn].fail_at > 0 &&
        (g_slot[fn].calls == g_slot[fn].fail_at ||
         (g_slot[fn].sticky && g_slot[fn].calls > g_slot[fn].fail_at)))
        err = g_slot[fn].err;
    pthread_mutex_unlock(&g_mu);
    return err;
}

void *t_malloc(size_t n)
{
    int e = hit(FI_MALLOC);
    if (e) { errno = e; return NULL; }
    return malloc(n);
}

char *t_strdup(const char *s)
{
    int e = hit(FI_STRDUP);
    if (e) { errno = e; return NULL; }
    return strdup(s);
}

int t_open(const char *path, int flags, ...)
{
    mode_t mode = 0;
    int e = hit(FI_OPEN);

    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    if (e) { errno = e; return -1; }
    return open(path, flags, mode);
}

ssize_t t_read(int fd, void *buf, size_t n)
{
    int e = hit(FI_READ);
    if (e) { errno = e; return -1; }
    if (g_read_max && n > g_read_max)
        n = g_read_max;
    return read(fd, buf, n);
}

ssize_t t_write(int fd, const void *buf, size_t n)
{
    int e = hit(FI_WRITE);
    if (e) { errno = e; return -1; }
    if (g_write_max && n > g_write_max)
        n = g_write_max;
    return write(fd, buf, n);
}

int t_close(int fd)
{
    int e = hit(FI_CLOSE);
    if (e) {                    /* really close it, but report failure */
        close(fd);
        errno = e;
        return -1;
    }
    return close(fd);
}

off_t t_lseek(int fd, off_t off, int whence)
{
    int e = hit(FI_LSEEK);
    if (e) { errno = e; return (off_t)-1; }
    return lseek(fd, off, whence);
}

int t_fstat(int fd, struct stat *st)
{
    int e = hit(FI_FSTAT);
    if (e) { errno = e; return -1; }
    return fstat(fd, st);
}

int t_stat(const char *path, struct stat *st)
{
    int e = hit(FI_STAT);
    if (e) { errno = e; return -1; }
    return stat(path, st);
}

int t_lstat(const char *path, struct stat *st)
{
    int e = hit(FI_LSTAT);
    int r;

    if (e) { errno = e; return -1; }
    r = lstat(path, st);
    if (r == 0 && g_flip_at > 0 && g_slot[FI_LSTAT].calls == g_flip_at)
        st->st_dev = ~st->st_dev;
    return r;
}

int t_mkdir(const char *path, mode_t mode)
{
    int e = hit(FI_MKDIR);
    if (e) { errno = e; return -1; }
    return mkdir(path, mode);
}

DIR *t_opendir(const char *path)
{
    int e = hit(FI_OPENDIR);
    if (e) { errno = e; return NULL; }
    return opendir(path);
}

int t_chmod(const char *path, mode_t mode)
{
    int e = hit(FI_CHMOD);
    if (e) { errno = e; return -1; }
    return chmod(path, mode);
}

int t_fchmod(int fd, mode_t mode)
{
    int e = hit(FI_FCHMOD);
    if (e) { errno = e; return -1; }
    return fchmod(fd, mode);
}

int t_pthread_create(pthread_t *t, const pthread_attr_t *a,
                     void *(*fn)(void *), void *arg)
{
    int e = hit(FI_PTHREAD);
    if (e) return e;            /* pthread_create returns the error code */
    return pthread_create(t, a, fn, arg);
}

/* A failing snprintf either returns -1 or behaves like a truncating one. */
int t_snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    int r;

    if (hit(FI_SNPRINTF)) {
        if (g_trunc_len > 0) {
            if (cap > 0) {
                memset(buf, 'A', cap - 1);
                buf[cap - 1] = '\0';
            }
            return g_trunc_len;
        }
        return -1;
    }
    va_start(ap, fmt);
    r = vsnprintf(buf, cap, fmt, ap);
    va_end(ap);
    return r;
}

int t_clock_gettime(clockid_t id, struct timespec *ts)
{
    if (g_clock_n > 0) {
        double v;

        pthread_mutex_lock(&g_mu);
        v = g_clock[g_clock_i < g_clock_n ? g_clock_i++ : g_clock_n - 1];
        pthread_mutex_unlock(&g_mu);
        ts->tv_sec = (time_t)v;
        ts->tv_nsec = (long)((v - (double)(time_t)v) * 1e9);
        return 0;
    }
    return clock_gettime(id, ts);
}

struct passwd *t_getpwuid(uid_t uid)
{
    if (hit(FI_GETPWUID))
        return NULL;
    return getpwuid(uid);
}
