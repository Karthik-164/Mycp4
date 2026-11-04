#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "mycp.h"

/* Copy src several times with different buffer sizes and print a table. */
int run_benchmark(const char *src, const char *dst, const options_t *opt);

#endif
