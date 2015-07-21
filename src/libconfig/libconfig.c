#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "liblog.h"
#include "libconfig.h"

static FILE *g_cfg_fp = NULL;

int32_t libconfig_open(char *cfg_path)
{
    if (NULL == cfg_path) {
        PRINTF(LEVEL_ERROR, "config file path is NULL.\n");
        return -1;
    }

    if (NULL != g_cfg_fp) {
        PRINTF(LEVEL_ERROR, "already opened a file, close it before a new open operation.\n");
        return -1;
    }

    g_cfg_fp = fopen(cfg_path, "rb");
    if (NULL == g_cfg_fp) {
        PRINTF(LEVEL_ERROR, "fopen error, path = [%s].\n", cfg_path);
        return -1;
    }

    return 0;
}

int32_t libconfig_close()
{
    if (NULL == g_cfg_fp) {
        PRINTF(LEVEL_DEBUG, "no config file opened.\n");
        return -1;
    }

    fclose(g_cfg_fp);

    return 0;
}

int32_t libconfig_get_cfg(char *key, char *value, int32_t value_len)
{
    char *line = NULL;
    char *real_line = NULL;
    char *tmp = NULL;
    size_t len = 0;
    size_t read;

    if (NULL == key || NULL == value || 0 == value_len) {
        PRINTF(LEVEL_ERROR, "argument error.\n");
        return -1;
    }

    if (NULL == g_cfg_fp) {
        PRINTF(LEVEL_ERROR, "no config file opened.\n");
        return -1;
    }

    fseek(g_cfg_fp, SEEK_SET, 0);
    memset(value, 0, value_len);

    while ((read = getline(&line, &len, g_cfg_fp)) != -1) {
        PRINTF(LEVEL_TEST, "%d:[%s]\n", read, line);

        real_line = line;

        // 去除行首空格/制表符等无意义字符
        while (0 != isspace((int32_t)*(real_line))) real_line++;

        // 注释行，跳过
        if ('#' == real_line[0])
            continue;

        tmp = strstr(real_line, key);

        // 此行无key，跳过
        if (NULL == tmp)
            continue;

        // 跳过key，准备计算value值
        real_line = tmp + strlen(key);

        // 去除等号左测空格/制表符等无意义字符
        while (0 != isspace((int32_t)*(real_line))) real_line++;
        // 必须是等号，是则跳过等号，否则跳过整行
        if ('=' != real_line[0])
            continue;
        else
            real_line++;
        // 去除等号右测空格/制表符等无意义字符
        while (0 != isspace((int32_t)*(real_line))) real_line++;
        
        // 统计有效字符长度
        tmp = real_line;
        while (0 != isprint((int32_t)*(real_line)) && 0 == isspace((int32_t)*(real_line))) real_line++;
        
        if ((real_line == tmp) || ((real_line - tmp) > value_len)) {
            PRINTF(LEVEL_WARNING, "value buf not enough or no valid value exsit.\n");
            continue;
        }

        strncpy(value, tmp, real_line - tmp);
        value[real_line - tmp] = '\0';

        if (line) {
            free(line);
        }

        return 0;
    }

    if (line) {
        free(line);
    }

    return -1;
}

int32_t get_cfg_from_file(char *key, char *value, int32_t value_len, char *cfg_path)
{
    FILE *fp = NULL;
    char *line = NULL;
    char *real_line = NULL;
    char *tmp = NULL;
    size_t len = 0;
    size_t read;

    if (NULL == key || NULL == value || 0 == value_len || NULL == cfg_path) {
        PRINTF(LEVEL_ERROR, "%s argument error.\n", __func__);
        return -1;
    }

    memset(value, 0, value_len);
    fp = fopen(cfg_path, "rb");
    if (NULL == fp) {
        PRINTF(LEVEL_ERROR, "%s fopen error, path = [%s].\n", __func__, cfg_path);
        return -1;
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        PRINTF(LEVEL_DEBUG, "%d:[%s]\n", read, line);

        real_line = line;

        // 去除行首空格/制表符等无意义字符
        while (0 != isspace((int32_t)*(real_line))) real_line++;

        // 注释行，跳过
        if ('#' == real_line[0])
            continue;

        tmp = strstr(real_line, key);

        // 此行无key，跳过
        if (NULL == tmp)
            continue;

        // 跳过key，准备计算value值
        real_line = tmp + strlen(key);

        // 去除等号左测空格/制表符等无意义字符
        while (0 != isspace((int32_t)*(real_line))) real_line++;
        // 必须是等号，是则跳过等号，否则跳过整行
        if ('=' != real_line[0])
            continue;
        else
            real_line++;
        // 去除等号右测空格/制表符等无意义字符
        while (0 != isspace((int32_t)*(real_line))) real_line++;
        
        // 统计有效字符长度
        tmp = real_line;
        while (0 != isprint((int32_t)*(real_line)) && 0 == isspace((int32_t)*(real_line))) real_line++;
        
        if ((real_line == tmp) || ((real_line - tmp) > value_len)) {
            PRINTF(LEVEL_WARNING, "value buf not enough or no valid value exsit.\n");
            continue;
        }

        strncpy(value, tmp, real_line - tmp);
        value[real_line - tmp] = '\0';

        PRINTF(LEVEL_DEBUG, "key:%s\tvalue:%s\n", key, value);
        fclose(fp);
        if (line) {
            free(line);
        }
        return 0;
    }

    fclose(fp);
    if (line) {
        free(line);
    }

    return -1;
}
