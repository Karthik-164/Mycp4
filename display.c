/* display.c - Level 3 file information display and Level 4 progress bar */
#include <stdio.h>
#include <time.h>
#include <pwd.h>
#include <sys/stat.h>

#include "display.h"
#include "utils.h"

#define BAR_WIDTH 30

/* ---------- Level 3: file information display ---------- */
void print_file_info(const char *label, const char *path,
                            const struct stat *st)
{
    char size[32], perm[11], mtime[32];
    struct tm tm;
    const struct passwd *pw = getpwuid(st->st_uid);
    mode_t m = st->st_mode;

    format_size((unsigned long long)st->st_size, size, sizeof(size));
    snprintf(perm, sizeof(perm), "%c%c%c%c%c%c%c%c%c%c",
             S_ISDIR(m) ? 'd' : S_ISREG(m) ? '-' : '?',
             (m & S_IRUSR) ? 'r' : '-', (m & S_IWUSR) ? 'w' : '-',
             (m & S_IXUSR) ? 'x' : '-', (m & S_IRGRP) ? 'r' : '-',
             (m & S_IWGRP) ? 'w' : '-', (m & S_IXGRP) ? 'x' : '-',
             (m & S_IROTH) ? 'r' : '-', (m & S_IWOTH) ? 'w' : '-',
             (m & S_IXOTH) ? 'x' : '-');
    localtime_r(&st->st_mtime, &tm);
    strftime(mtime, sizeof(mtime), "%Y-%m-%d %H:%M:%S", &tm);

    printf("---- %s file info ----\n", label);
    printf("  Path        : %s\n", path);
    printf("  Size        : %lld bytes (%s)\n", (long long)st->st_size, size);
    printf("  Permissions : %s (%04o)\n", perm, (unsigned)(m & 07777));
    printf("  Owner       : %s (uid %u)\n", pw ? pw->pw_name : "?", (unsigned)st->st_uid);
    printf("  Inode       : %llu\n", (unsigned long long)st->st_ino);
    printf("  Hard links  : %lu\n", (unsigned long)st->st_nlink);
    printf("  Modified    : %s\n", mtime);
}

/* ---------- Level 4: progress bar (single-threaded use only) ---------- */
static int g_last_pct = -1;

void progress_start(void)
{
    g_last_pct = -1;
}

void progress_update(unsigned long long done, unsigned long long total)
{
    char bar[BAR_WIDTH + 1], d[32], t[32];
    int pct, filled, i;

    if (total == 0)
        return;
    pct = (int)((double)done * 100.0 / (double)total);
    if (pct > 100)
        pct = 100;
    if (pct == g_last_pct)
        return;
    g_last_pct = pct;

    filled = pct * BAR_WIDTH / 100;
    for (i = 0; i < BAR_WIDTH; i++)
        bar[i] = (i < filled) ? '#' : '.';
    bar[BAR_WIDTH] = '\0';

    format_size(done, d, sizeof(d));
    format_size(total, t, sizeof(t));
    printf("\r[%s] %3d%%  %s / %s", bar, pct, d, t);
    fflush(stdout);
}

void progress_finish(void)
{
    if (g_last_pct >= 0) {
        printf("\n");
        fflush(stdout);
    }
    g_last_pct = -1;
}
