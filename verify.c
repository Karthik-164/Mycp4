/* verify.c - copy verification (byte-by-byte comparison) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "verify.h"
#include "utils.h"
#include "logger.h"

/* ---------- copy verification: byte-by-byte compare ---------- */
/* Returns 0 identical, 1 different, -1 error. */
int verify_files(const char *a, const char *b, size_t bufsize)
{
    int fa = open(a, O_RDONLY), fb = open(b, O_RDONLY), result = 0;
    char *ba = malloc(bufsize), *bb = malloc(bufsize);

    if (fa < 0 || fb < 0 || !ba || !bb) {
        report_error("verify: cannot open files or allocate memory");
        result = -1;
    }
    while (result == 0) {
        ssize_t na = read_full(fa, ba, bufsize);
        ssize_t nb = read_full(fb, bb, bufsize);

        if (na < 0 || nb < 0) {
            report_error("verify: read error: %s", strerror(errno));
            result = -1;
        } else if (na != nb || memcmp(ba, bb, (size_t)na) != 0) {
            result = 1;
        } else if (na == 0) {
            break;
        }
    }
    free(ba);
    free(bb);
    if (fa >= 0) close(fa);
    if (fb >= 0) close(fb);
    return result;
}
