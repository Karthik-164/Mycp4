/* benchmark.c - copy performance benchmark (different buffer sizes) */
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include "benchmark.h"
#include "copy.h"
#include "logger.h"
#include "utils.h"

/* Copy the file with several buffer sizes and print a comparison table. */
int run_benchmark(const char *src, const char *dst, const options_t *opt)
{
    static const size_t sizes[] = { 512, 1024, 4096, 8192, 16384, 65536, 262144, 1048576 };
    const size_t count = sizeof(sizes) / sizeof(sizes[0]);
    char tmp[PATH_MAX], label[32], total[32];
    double secs[8];
    size_t i, best = 0;
    struct stat st;

    if (snprintf(tmp, sizeof(tmp), "%s.bench.tmp", dst) >= (int)sizeof(tmp) ||
        stat(src, &st) < 0) {
        report_error("benchmark: bad source or destination path");
        return -1;
    }
    format_size((unsigned long long)st.st_size, total, sizeof(total));
    printf("Benchmark: copying '%s' (%s) with different buffer sizes\n", src, total);
    printf("%-12s %-12s %s\n", "Buffer", "Time (s)", "Speed (MB/s)");
    printf("------------ ------------ ------------\n");

    for (i = 0; i < count; i++) {
        options_t o = *opt;
        stats_t s = {0};
        double mbps;

        o.buf_size = sizes[i];
        o.progress = o.show_info = o.verify = o.resume = 0;
        if (copy_file(src, tmp, &o, &s) < 0) {
            unlink(tmp);
            return -1;
        }
        unlink(tmp);

        secs[i] = s.seconds;
        mbps = s.seconds > 0 ? ((double)s.bytes / 1048576.0) / s.seconds : 0.0;
        format_size(sizes[i], label, sizeof(label));
        printf("%-12s %-12.4f %.2f\n", label, s.seconds, mbps);
        log_info("benchmark buffer=%zu time=%.4fs speed=%.2fMB/s", sizes[i], s.seconds, mbps);
        if (secs[i] < secs[best])
            best = i;
    }
    format_size(sizes[best], label, sizeof(label));
    printf("\nFastest buffer size: %s\n", label);
    return 0;
}
