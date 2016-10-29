#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>

#define log(level, format, ...) \
    do { \
        syslog(level, "[%s,%s(),%d,"#level"] "format, \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__ ); \
    } while (0)

    #ifndef NO_LOG_ERROR
        #define log_error(...) log(LOG_ERR, __VA_ARGS__)
    #else
        #define log_error(...)
    #endif

    #ifndef NO_LOG_WARNING
        #define log_warning(...) log(LOG_WARNING, __VA_ARGS__)
    #else
        #define log_warning(...)
    #endif

    #ifndef NO_LOG_INFO
        #define log_info(...) log(LOG_INFO, __VA_ARGS__)
    #else
        #define log_info(...)
    #endif

    #ifndef NO_LOG_DEBUG
        #define log_debug(...) log(LOG_DEBUG, __VA_ARGS__)
    #else
        #define log_debug(...)
    #endif

#define log_init() \
    do { \
        openlog("MASTER", 0, LOG_LOCAL1); \ //it will return true for all time
    } while (0)

#define log_free() \
    do { \
        closelog(); \
    } while (0)


#endif //_LOG_H_