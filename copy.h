#ifndef COPY_H
#define COPY_H

#include "mycp.h"

/* Copy one regular file using open/read/write/lseek/close.
 * Returns 0 on success, -1 on failure (message already reported). */
int copy_file(const char *src, const char *dst,
              const options_t *opt, stats_t *stats);

#endif
