#ifndef MYCP_H
#define MYCP_H

#include <stddef.h>

#define DEFAULT_BUF_SIZE 4096
#define MAX_BUF_SIZE     (256UL * 1024 * 1024)
#define MAX_THREADS      64

/* Command-line options shared by all modules */
typedef struct {
    size_t      buf_size;    /* Level 2: buffer size in bytes       */
    int         show_info;   /* Level 3: file information display   */
    int         progress;    /* Level 4: progress bar               */
    int         preserve;    /* Level 5: preserve permissions       */
    int         recursive;   /* Ext: recursive directory copy       */
    int         benchmark;   /* Ext: copy performance benchmark     */
    int         verify;      /* Ext: copy verification              */
    int         resume;      /* Ext: resume interrupted copy        */
    int         threads;     /* Ext: parallel copy (pthreads)       */
    const char *log_path;    /* Ext: logging system (NULL = off)    */
} options_t;

/* Performance statistics */
typedef struct {
    unsigned long long bytes;
    double             seconds;
    unsigned long      files_ok;
    unsigned long      files_failed;
} stats_t;

#endif
