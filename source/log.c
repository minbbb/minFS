#include <stdio.h>
#include <stdarg.h>
#include <time.h>

static void log_timestamp(FILE *f)
{
    struct timespec ts;
    struct tm tm;

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return;

    gmtime_r(&ts.tv_sec, &tm);
    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
}

#ifdef SYSMODULE
#include <sys/stat.h>

#include "config.h"
#include "log.h"

static int log_enabled(void)
{
#ifdef ENABLE_LOGGING
    return 1;
#else
    struct stat st;
    return stat(LOG_FLAG_PATH, &st) == 0;
#endif
}

void log_file(const char *fmt, ...)
{
    if (!log_enabled())
        return;

    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (f == NULL)
        return;

    log_timestamp(f);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fclose(f);
}
#else
#include "log.h"

void log_debug(const char *fmt, ...)
{
    log_timestamp(stdout);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
#endif