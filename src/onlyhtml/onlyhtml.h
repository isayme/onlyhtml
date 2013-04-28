#ifndef _ONLYHTML_H
#define _ONLYHTML_H

#include <sys/epoll.h>

#include "defs.h"

#define HTTP_CUR_DIR "./"
#define HTTP_HEADER_SPACE " \r\n"

#define HTTP_REQ_GET "GET"

#define ONLYHTML_SERVERNAME "onlyhtml"
#define ONLYHTML_VERSION "1.0"

#define HTTP_RESPOND(code, type, content) \
"HTTP/1.1 "code"\r\n"\
"Server: "ONLYHTML_SERVERNAME"/"ONLYHTML_VERSION"\r\n"\
"Connection: Close\r\n\r\n"\
content

#define HTTP_FILE_HTML "text/html"
#define HTTP_FILE_IMAGE "image/jpeg"

#define HTTP_RESPOND_404 HTTP_RESPOND("404 Not Found", HTTP_FILE_HTML, \
    "<html><head><title>404</title></head><body><h1>Sorry, 404</h1></body></html>")

#define HTTP_RESPOND_200(type) HTTP_RESPOND("200 OK", type, "")


#pragma pack(1)

typedef struct onlyhtml_info_t {
    time_t start_time;
#define ONLYHTML_DEF_PORT 80
    UINT16 port;
    int sockfd;
    int efd;
#define EVENTS_NUMBER 256
    struct epoll_event events[EVENTS_NUMBER];

#define STATE_PREPARE 0
#define STATE_RUNNING 1
#define STATE_STOP 2
    INT32 state;

#define HTTP_DEF_ROOT_DIR_LEN 128
#define HTTP_DEF_ROOT_DIR "/var/www"
    char root[HTTP_DEF_ROOT_DIR_LEN];

#define HTTP_DEF_404_PAGE_LEN 128
#define HTTP_DEF_404_PAGE "404.html"
    char e404[HTTP_DEF_404_PAGE_LEN];

#define HTTP_DEF_INDEX "/index.html"
}onlyhtml_info_t;

#pragma pack()

#define ERR_STR strerror(errno)

#endif