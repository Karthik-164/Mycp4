/* utils.c - read/write loops and size / time / path helpers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "mycp.h"
#include "utils.h"

/* ---------- I/O helpers ---------- */

/* Write exactly 'count' bytes (handles partial writes and EINTR). */
int write_all(int fd, const void *buf, size_t count)
{
    const char *p = buf;
    size_t done = 0;

    while (done < count) {
        ssize_t n = write(fd, p + done, count - done);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

/* Read until 'count' bytes or EOF. */
ssize_t read_full(int fd, void *buf, size_t count)
{
    char *p = buf;
    size_t got = 0;

    while (got < count) {
        ssize_t n = read(fd, p + got, count - got);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            break;
        got += (size_t)n;
    }
    return (ssize_t)got;
}

/* ---------- size / time / path helpers ---------- */

/* "4096", "64K", "1M" -> bytes. Returns 0 / -1. */
int parse_size(const char *text, size_t *out)
{
    char *end;
    unsigned long long v;

    if (text[0] < '0' || text[0] > '9')
        return -1;
    errno = 0;
    v = strtoull(text, &end, 10);
    if (errno != 0 || v == 0 || v > MAX_BUF_SIZE)
        return -1;

    if (*end != '\0') {
        if (end[1] != '\0')
            return -1;
        if (*end == 'k' || *end == 'K')
            v *= 1024ULL;
        else if (*end == 'm' || *end == 'M')
            v *= 1024ULL * 1024ULL;
        else
            return -1;
    }
    if (v > MAX_BUF_SIZE)
        return -1;
    *out = (size_t)v;
    return 0;
}

void format_size(unsigned long long bytes, char *out, size_t cap)
{
    double v = (double)bytes;
    int u = 0;

    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        u++;
    }
    if (u == 0) {
        snprintf(out, cap, "%llu B", bytes);
    } else {
        static const char *const units[] = { "B", "KB", "MB", "GB", "TB" };
        snprintf(out, cap, "%.2f %s", v, units[u]);
    }
}

double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int join_path(char *out, size_t cap, const char *dir, const char *name)
{
    size_t len = strlen(dir);
    int n = snprintf(out, cap, (len > 0 && dir[len - 1] == '/') ? "%s%s" : "%s/%s",
                     dir, name);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}
