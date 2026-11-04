#ifndef LOGGER_H
#define LOGGER_H

/* Open (append) a log file. Returns 0 / -1. */
int  log_open(const char *path);
void log_close(void);

/* Timestamped line in the log file (no-op if no log is open). */
void log_info(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Print "mycp: <msg>" to stderr and write it to the log as ERROR. */
void report_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif
