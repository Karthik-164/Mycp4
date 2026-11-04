#ifndef VERIFY_H
#define VERIFY_H

#include <stddef.h>

/* Byte-by-byte comparison. Returns 0 identical, 1 different, -1 error. */
int verify_files(const char *a, const char *b, size_t bufsize);

#endif
