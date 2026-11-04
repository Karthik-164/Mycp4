#ifndef DIRCOPY_H
#define DIRCOPY_H

#include "mycp.h"

/* Recursively copy directory src into dst (optionally with threads). */
int copy_directory(const char *src, const char *dst,
                   const options_t *opt, stats_t *stats);

#endif
