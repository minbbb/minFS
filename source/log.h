#ifndef LOG_H
#define LOG_H

#ifdef SYSMODULE
    void log_file(const char *fmt, ...);
    #define log_debug(...) log_file(__VA_ARGS__)
#else
    #define log_file(...) ((void)0)
    void log_debug(const char *fmt, ...);
#endif

#endif