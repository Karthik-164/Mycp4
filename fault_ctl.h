/* fault_ctl.h - control API for the fault-injection layer (test only). */
#ifndef FAULT_CTL_H
#define FAULT_CTL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FI_MALLOC, FI_STRDUP, FI_OPEN, FI_READ, FI_WRITE, FI_CLOSE, FI_LSEEK,
    FI_FSTAT, FI_STAT, FI_LSTAT, FI_MKDIR, FI_OPENDIR, FI_CHMOD, FI_FCHMOD,
    FI_PTHREAD, FI_SNPRINTF, FI_GETPWUID, FI_COUNT
} fi_fn_t;

void fi_reset(void);                                  /* clear everything      */
void fi_fail_at(fi_fn_t fn, int nth, int err);        /* only the nth call     */
void fi_fail_from(fi_fn_t fn, int nth, int err);      /* nth call and all later */
int  fi_calls(fi_fn_t fn);                            /* calls seen since reset */
void fi_io_chunk(size_t read_max, size_t write_max);  /* force short read/write */
void fi_snprintf_truncate(int reported_len);          /* faulty snprintf behaves like a truncating one */
void fi_clock_script(const double *vals, int n);      /* scripted clock_gettime */
void fi_lstat_flip_dev_at(int nth);                   /* nth lstat reports another st_dev */

#ifdef __cplusplus
}
#endif
#endif
