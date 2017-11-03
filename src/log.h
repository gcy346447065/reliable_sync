#ifndef _LOG_H_
#define _LOG_H_

#include <syslog.h>
#include <libgen.h>

#define WITH_LOG

#ifdef WITH_LOG
    #define log(level, format, ...) \
        do { \
            syslog(level, "[%s,%s(),%d,"#level"] "format, \
                basename((char *)__FILE__), __func__, __LINE__, ##__VA_ARGS__ ); \
        } while (0)

    #define log_error(...) log(LOG_ERR, __VA_ARGS__)
    #define log_warning(...) log(LOG_WARNING, __VA_ARGS__)
    #define log_info(...) log(LOG_INFO, __VA_ARGS__)
    #define log_debug(...) log(LOG_DEBUG, __VA_ARGS__)

    #define log_hex(buf, buflen) \
                do { \
                    if(buflen == 0) \
                    { \
                        break; \
                    } \
                    syslog(LOG_DEBUG, "    01 02 03 04 05 06 07 08 09 10"); \
                    int line = 0; \
                    while(line < buflen / 10) \
                    { \
                        syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", \
                            (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, \
                            *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff, *((char *)buf+line*10+8) & 0xff, *((char *)buf+line*10+9) & 0xff); \
                        line++; \
                    } \
                    switch(buflen % 10) \
                    { \
                        case 0: \
                            break; \
                        case 1: \
                            syslog(LOG_DEBUG, "%02d: %02x", (int)(line+1), *((char *)buf+line*10) & 0xff); \
                            break; \
                        case 2: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff); \
                            break; \
                        case 3: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff); \
                            break; \
                        case 4: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff); \
                            break; \
                        case 5: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff); \
                            break; \
                        case 6: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff); \
                            break; \
                        case 7: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff); \
                            break; \
                        case 8: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff); \
                            break; \
                        case 9: \
                            syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff, *((char *)buf+line*10+8) & 0xff); \
                            break; \
                    } \
                }while (0)

    #define log_hex_8(buf, buflen) \
                                        do { \
                                            if(buflen == 0) \
                                            { \
                                                break; \
                                            } \
                                            syslog(LOG_DEBUG, "    01 02 03 04 05 06 07 08"); \
                                            int line = 0; \
                                            while(line < buflen / 8) \
                                            { \
                                                syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x %02x %02x", \
                                                    (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, \
                                                    *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff, *((char *)buf+line*10+7) & 0xff); \
                                                line++; \
                                            } \
                                            switch(buflen % 8) \
                                            { \
                                                case 0: \
                                                    break; \
                                                case 1: \
                                                    syslog(LOG_DEBUG, "%02d: %02x", (int)(line+1), *((char *)buf+line*10) & 0xff); \
                                                    break; \
                                                case 2: \
                                                    syslog(LOG_DEBUG, "%02d: %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff); \
                                                    break; \
                                                case 3: \
                                                    syslog(LOG_DEBUG, "%02d: %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff); \
                                                    break; \
                                                case 4: \
                                                    syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff); \
                                                    break; \
                                                case 5: \
                                                    syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff); \
                                                    break; \
                                                case 6: \
                                                    syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff); \
                                                    break; \
                                                case 7: \
                                                    syslog(LOG_DEBUG, "%02d: %02x %02x %02x %02x %02x %02x %02x", (int)(line+1), *((char *)buf+line*10) & 0xff, *((char *)buf+line*10+1) & 0xff, *((char *)buf+line*10+2) & 0xff, *((char *)buf+line*10+3) & 0xff, *((char *)buf+line*10+4) & 0xff, *((char *)buf+line*10+5) & 0xff, *((char *)buf+line*10+6) & 0xff); \
                                                    break; \
                                            } \
                                        }while (0)


#else
    #define log_error(...)
    #define log_warning(...)
    #define log_info(...)
    #define log_debug(...)
    #define log_hex(buf, buflen)
    #define log_hex_8(buf, buflen)
#endif

//it will return true for all time
#define log_init(label, local) \
    do { \
        if(local == 1) \
            openlog(label, 0, LOG_LOCAL1); \
        else if(local == 2) \
            openlog(label, 0, LOG_LOCAL2); \
        else if(local == 3) \
            openlog(label, 0, LOG_LOCAL3); \
        else if(local == 4) \
            openlog(label, 0, LOG_LOCAL4); \
    } while (0)

#define log_free() \
    do { \
        closelog(); \
    } while (0)

#endif //_LOG_H_

