#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include <libgen.h>

#define WITH_LOG

#define LOG1 1
#define LOG2 2
#define LOG3 3
#define LOG4 4

#ifdef WITH_LOG
    #define log(local, level, format, ...) \
        do { \
            syslog(local | level, "[%s,%s(),%d,"#level"] "format, \
                basename((char *)__FILE__), __func__, __LINE__, ##__VA_ARGS__ ); \
        } while (0)

    #define log_error(num, ...) \
        do { \
            if(num == LOG1) \
                log(LOG_LOCAL1, LOG_ERR, __VA_ARGS__); \
            else if(num == LOG2) \
                log(LOG_LOCAL2, LOG_ERR, __VA_ARGS__); \
            else if(num == LOG3) \
                log(LOG_LOCAL3, LOG_ERR, __VA_ARGS__); \
            else if(num == LOG4) \
                log(LOG_LOCAL4, LOG_ERR, __VA_ARGS__); \
        } while (0)
    
    #define log_warning(num, ...) \
        do { \
            if(num == LOG1) \
                log(LOG_LOCAL1, LOG_WARNING, __VA_ARGS__); \
            else if(num == LOG2) \
                log(LOG_LOCAL2, LOG_WARNING, __VA_ARGS__); \
            else if(num == LOG3) \
                log(LOG_LOCAL3, LOG_WARNING, __VA_ARGS__); \
            else if(num == LOG4) \
                log(LOG_LOCAL4, LOG_WARNING, __VA_ARGS__); \
        } while (0)
        
    #define log_info(num, ...) \
        do { \
            if(num == LOG1) \
                log(LOG_LOCAL1, LOG_INFO, __VA_ARGS__); \
            else if(num == LOG2) \
                log(LOG_LOCAL2, LOG_INFO, __VA_ARGS__); \
            else if(num == LOG3) \
                log(LOG_LOCAL3, LOG_INFO, __VA_ARGS__); \
            else if(num == LOG4) \
                log(LOG_LOCAL4, LOG_INFO, __VA_ARGS__); \
        } while (0)

    #define log_debug(num, ...) \
        do { \
            if(num == LOG1) \
                log(LOG_LOCAL1, LOG_DEBUG, __VA_ARGS__); \
            else if(num == LOG2) \
                log(LOG_LOCAL2, LOG_DEBUG, __VA_ARGS__); \
            else if(num == LOG3) \
                log(LOG_LOCAL3, LOG_DEBUG, __VA_ARGS__); \
            else if(num == LOG4) \
                log(LOG_LOCAL4, LOG_DEBUG, __VA_ARGS__); \
        } while (0)

    #define log_hex(num, buf, buflen) \
        do { \
            if(buflen == 0) \
            { \
                break; \
            } \
            int flag = 0; \
            if(num == LOG1) \
                flag = LOG_DEBUG | LOG_LOCAL1; \
            else if(num == LOG2) \
                flag = LOG_DEBUG | LOG_LOCAL2; \
            else if(num == LOG3) \
                flag = LOG_DEBUG | LOG_LOCAL3; \
            else if(num == LOG4) \
                flag = LOG_DEBUG | LOG_LOCAL4; \
            syslog(flag, "    01 02 03 04 05 06 07 08 09 10"); \
            int line = 0; \
            while(line < buflen / 10) \
            { \
                syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", \
                    (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, \
                    *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff, *((char *)buf+line*10+8) & 0xff, *((char *)buf+line*10+9) & 0xff); \
                line++; \
            } \
            switch(buflen % 10) \
            { \
                case 0: \
                    break; \
                case 1: \
                    syslog(flag, "%02d: %02x", (int)(line+1), *((char *)buf+line*10) & 0xff); \
                    break; \
                case 2: \
                    syslog(flag, "%02d: %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff); \
                    break; \
                case 3: \
                    syslog(flag, "%02d: %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff); \
                    break; \
                case 4: \
                    syslog(flag, "%02d: %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff); \
                    break; \
                case 5: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff); \
                    break; \
                case 6: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff); \
                    break; \
                case 7: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff); \
                    break; \
                case 8: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff); \
                    break; \
                case 9: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff, *((char *)buf+line*10+8) & 0xff); \
                    break; \
            } \
        }while (0)
    #define log_hex_8(num, buf, buflen) \
        do { \
            if(buflen == 0) \
            { \
                break; \
            } \
            int flag = 0; \
            if(num == LOG1) \
                flag = LOG_DEBUG | LOG_LOCAL1; \
            else if(num == LOG2) \
                flag = LOG_DEBUG | LOG_LOCAL2; \
            else if(num == LOG3) \
                flag = LOG_DEBUG | LOG_LOCAL3; \
            else if(num == LOG4) \
                flag = LOG_DEBUG | LOG_LOCAL4; \
            syslog(flag, "    01 02 03 04 05 06 07 08"); \
            int line = 0; \
            while(line < buflen / 8) \
            { \
                syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x", \
                    (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, \
                    *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff); \
                line++; \
            } \
            switch(buflen % 8) \
            { \
                case 0: \
                    break; \
                case 1: \
                    syslog(flag, "%02d: %02x", (int)(line+1), *((char *)buf+line*10) & 0xff); \
                    break; \
                case 2: \
                    syslog(flag, "%02d: %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff); \
                    break; \
                case 3: \
                    syslog(flag, "%02d: %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff); \
                    break; \
                case 4: \
                    syslog(flag, "%02d: %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff); \
                    break; \
                case 5: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff); \
                    break; \
                case 6: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff); \
                    break; \
                case 7: \
                    syslog(flag, "%02d: %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff); \
                    break; \
            } \
        }while (0)

#else
    #define log_error(...)
    #define log_warning(...)
    #define log_info(...)
    #define log_debug(...)
    #define log_hex(...)
    #define log_hex_8(...)
#endif

//it will return true for all time
#define log_init(num, label) \
    do { \
        if(num == LOG1) \
            openlog(label, 0, LOG_LOCAL1); \
        else if(num == LOG2) \
            openlog(label, 0, LOG_LOCAL2); \
        else if(num == LOG3) \
            openlog(label, 0, LOG_LOCAL3); \
        else if(num == LOG4) \
            openlog(label, 0, LOG_LOCAL4); \
    } while (0)

#define log_free() \
    do { \
        closelog(); \
    } while (0)

#endif //_LOG_H_

