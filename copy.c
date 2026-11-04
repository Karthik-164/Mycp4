/* copy.c - single file copy: Levels 1-5 and resume.
 * Uses display.c (info + progress), verify.c and logger.c. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "copy.h"
#include "display.h"
#include "verify.h"
#include "logger.h"
#include "utils.h"

/* ---------- the copy itself ---------- */
int copy_file(const char *src, const char *dst,
              const options_t *opt, stats_t *stats)
{
    struct stat sst, dstat;
    int in = -1, out = -1, ret = -1, have_dst, rc, v;
    char *buf = NULL;
    unsigned long long offset = 0, copied = 0, total;
    double t0 = now_seconds();
    ssize_t n;

    /* open and inspect the source */
    in = open(src, O_RDONLY);
    if (in < 0) {
        report_error("cannot open '%s': %s", src, strerror(errno));
        goto cleanup;
    }
    if (fstat(in, &sst) < 0) {
        report_error("cannot stat '%s': %s", src, strerror(errno));
        goto cleanup;
    }
    if (!S_ISREG(sst.st_mode)) {
        report_error("'%s' is not a regular file", src);
        goto cleanup;
    }
    total = (unsigned long long)sst.st_size;

    /* refuse to copy a file onto itself */
    have_dst = (stat(dst, &dstat) == 0);
    if (have_dst && dstat.st_dev == sst.st_dev && dstat.st_ino == sst.st_ino) {
        report_error("'%s' and '%s' are the same file", src, dst);
        goto cleanup;
    }

    /* resume: continue from the size of a partial destination */
    if (opt->resume && have_dst && S_ISREG(dstat.st_mode) &&
        (unsigned long long)dstat.st_size <= total)
        offset = (unsigned long long)dstat.st_size;

    if (opt->show_info)
        print_file_info("Source", src, &sst);

    /* open the destination */
    out = open(dst, O_WRONLY | O_CREAT | (offset ? 0 : O_TRUNC), 0644);
    if (out < 0) {
        report_error("cannot create '%s': %s", dst, strerror(errno));
        goto cleanup;
    }
    if (offset > 0) {
        if (lseek(in, (off_t)offset, SEEK_SET) < 0 ||
            lseek(out, (off_t)offset, SEEK_SET) < 0) {
            report_error("lseek failed: %s", strerror(errno));
            goto cleanup;
        }
        printf("Resuming '%s' from byte %llu of %llu\n", dst, offset, total);
        log_info("resuming %s from byte %llu", dst, offset);
    }

    buf = malloc(opt->buf_size);
    if (!buf) {
        report_error("out of memory (buffer %zu bytes)", opt->buf_size);
        goto cleanup;
    }

    /* main loop: read a block, write the whole block */
    if (opt->progress)
        progress_start();
    for (;;) {
        n = read(in, buf, opt->buf_size);
        if (n == 0)
            break;                                  /* end of file */
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0) {
            if (opt->progress) progress_finish();
            report_error("read error on '%s': %s", src, strerror(errno));
            goto cleanup;
        }
        if (write_all(out, buf, (size_t)n) < 0) {
            if (opt->progress) progress_finish();
            report_error("write error on '%s': %s", dst, strerror(errno));
            goto cleanup;
        }
        copied += (unsigned long long)n;
        if (opt->progress)
            progress_update(offset + copied, total);
    }
    if (opt->progress)
        progress_finish();

    /* Level 5: preserve permissions */
    if (opt->preserve && fchmod(out, sst.st_mode & 07777) < 0) {
        report_error("cannot set permissions on '%s': %s", dst, strerror(errno));
        goto cleanup;
    }
    rc = close(out);
    out = -1;
    if (rc < 0) {
        report_error("close error on '%s': %s", dst, strerror(errno));
        goto cleanup;
    }

    if (opt->show_info && stat(dst, &dstat) == 0)
        print_file_info("Destination", dst, &dstat);

    if (opt->verify) {
        v = verify_files(src, dst, opt->buf_size);
        if (v == 1)
            report_error("verification FAILED: '%s' differs from '%s'", dst, src);
        if (v != 0)
            goto cleanup;
        printf("Verified: '%s' matches '%s'\n", dst, src);
    }

    log_info("copied %s -> %s (%llu bytes)", src, dst, copied);
    ret = 0;

cleanup:
    free(buf);
    if (in >= 0)
        close(in);
    if (out >= 0)
        close(out);
    if (stats) {
        stats->bytes = copied;
        stats->seconds = now_seconds() - t0;
    }
    return ret;
}
