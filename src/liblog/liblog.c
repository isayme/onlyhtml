#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <libgen.h>
#define __USE_GNU 
#include <pthread.h>

#include "defs.h"
#include "liblog.h"

static const uint8_t g_debug_str[8][6] = {
    "TESTS",
    "DEBUG",
    "INFOM",
    "ALARM",
    "WARNS",
    "ERROR",
    "UNDEF",
    "UNDEF",
};

static FILE *g_logfile = NULL;
static uint64_t g_dlevel = LEVEL_INFORM;
static uint32_t g_range_srart = 0;
static uint32_t g_range_end = -1;

static int32_t log_init()
{
    char *log_file_path;
    char *log_path;
    time_t log_t = 0;
    struct tm log_tm = {0};
    char *tmp;
    int32_t ret = -1;
    
    if (NULL == g_logfile) {
        log_path = malloc(PATH_MAX);
        if (NULL == log_path) {
            goto _err;
        }
        log_file_path = malloc(PATH_MAX);
        if (NULL == log_file_path) {
            goto _err;
        }

        memset((void*)log_path, 0x00, PATH_MAX);
        memset((void*)log_file_path, 0x00, PATH_MAX);
        
        if (-1 == readlink("/proc/self/exe", log_path, PATH_MAX)) {
            goto _err;
        }    
        tmp = dirname(log_path);

        snprintf(log_file_path, PATH_MAX, "%s/%s", tmp, log_file_path);
        
        time(&log_t);
        localtime_r(&log_t, &log_tm);
        snprintf(log_file_path, PATH_MAX, "%s/"LIBLOG_PREFIX"-%04d-%02d-%02d-%02d-%02d.log", tmp, 
            log_tm.tm_year+1900, log_tm.tm_mon + 1, log_tm.tm_mday, log_tm.tm_hour, log_tm.tm_min);
        
        g_logfile = fopen(log_file_path, "a");
        
        if (NULL == g_logfile) {
            goto _err;
        }
        
        printf("\r");
        liblog_log(LEVEL_DEBUG | COLOR_GREEN, "open log file %s ok.\n", log_file_path);
    }
    
    ret = 0;

_err:
    if (NULL != log_file_path) {
        free(log_file_path);
    }
    if (NULL != log_path) {
        free(log_path);
    }
    return ret;
}

int32_t liblog_level(uint64_t level)
{
    g_dlevel = level & LEVEL_MASK;

    return g_dlevel;
}

void liblog_range(uint32_t start, uint32_t end)
{
    g_range_srart = start;
    g_range_end = end;
}

uint32_t liblog_range_start()
{
    return g_range_srart;
}

uint32_t liblog_range_end()
{
    return g_range_end;
}

int32_t liblog_log(uint64_t mode, char *format, ...)
{
    int8_t str[1024] = {0};
    uint64_t dlevel = -1;
    va_list arg;

    dlevel = mode & LEVEL_MASK;
      
    if (g_dlevel > dlevel) {
        return 0;
    }
    
    if (LEVEL_ERROR == dlevel) {
        mode &= COLOR_MASK;
        mode |= COLOR_RED;
    }

    // ensure logfile has been opened
    if (NULL == g_logfile) {
        if (-1 == log_init()) {
            printf("init log file error\n");
            return -1;
        }
    }
    
    // show different color for shell console
    if (1 == isatty(STDOUT_FILENO)) {
        int8_t color = (int8_t)((mode & COLOR_MASK) >> COLOR_POS);
        if (0 == color) color = 9;
        fprintf(stdout, "\033[%d;49;%dm", (int8_t)((mode & TEXT_MASK) >> TEXT_POS), 30 + color);
    }

    // show time
    if (TIME_SHOW == (mode & TIME_MASK)) {
        char cur_time[128];
        struct tm ptm = {0};
        time_t t = time(NULL);

        localtime_r(&t, &ptm);
        strftime(cur_time, 128, "[%H:%M:%S %Y-%m-%d] ",&ptm);
        fprintf(stdout, "%s", cur_time);
        fprintf(g_logfile, "%s", cur_time);
    }
    
    // show debug level info
    fprintf(g_logfile, "[%s] ", g_debug_str[dlevel >> LEVEL_POS]);
    fprintf(stdout, "[%s] ", g_debug_str[dlevel >> LEVEL_POS]); 
    
    va_start(arg, format);
    vfprintf(stdout, format, arg);
    va_end(arg);
    va_start(arg, format);
    vfprintf(g_logfile, format, arg);
    va_end(arg);
    
    if (1 == isatty(STDOUT_FILENO)) {
        fprintf(stdout, "\033[0m");
    }
    
    fflush(stdout);
    fflush(g_logfile);
    
    return 0;
}
