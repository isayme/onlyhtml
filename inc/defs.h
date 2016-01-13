#ifndef _DEFS_H
#define _DEFS_H

#define LEVEL_TEST      "TESTS"
#define LEVEL_DEBUG     "DEBUG"
#define LEVEL_INFORM    "INFOM"
#define LEVEL_ALARM     "ALARM"
#define LEVEL_WARNING   "WARN"
#define LEVEL_ERROR     "ERROR"

#define PRINTF(level, format, ...) \
    do { \
        fprintf(stderr, "[%s] [%s@%s,%d] " format, \
            level, __func__, __FILE__, __LINE__, ##__VA_ARGS__ ); \
    } while (0)


#endif
