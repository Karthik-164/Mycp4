/* logger.c - thread-safe logging system (append-mode log file) */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "logger.h"
#include "utils.h"

static int             g_fd = -1;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static void log_line(const char *level, const char *msg)
{
    char ts[32], line[2048];
    struct tm tm;
    time_t now;
    int n;

    if (g_fd < 0)
        return;
    now = time(NULL);
    localtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    n = snprintf(line, sizeof(line), "[%s] %-5s %s\n", ts, level, msg);
    if (n < 0)
        return;
    if ((size_t)n >= sizeof(line)) {
        n = (int)sizeof(line) - 1;
        line[n - 1] = '\n';
    }
    pthread_mutex_lock(&g_lock);
    write_all(g_fd, line, (size_t)n);
    pthread_mutex_unlock(&g_lock);
}

int log_open(const char *path)
{
    g_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (g_fd < 0) {
        fprintf(stderr, "mycp: cannot open log file '%s': %s\n",
                path, strerror(errno));
        return -1;
    }
    return 0;
}

void log_close(void)
{
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

void log_info(const char *fmt, ...)
{
    char msg[1536];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    log_line("INFO", msg);
}

/* Print "mycp: <msg>" to stderr and log it as ERROR. */
void report_error(const char *fmt, ...)
{
    char msg[1536];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    fprintf(stderr, "mycp: %s\n", msg);
    log_line("ERROR", msg);
}
