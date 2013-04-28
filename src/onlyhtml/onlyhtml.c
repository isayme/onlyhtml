#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <errno.h>
#include <netdb.h>
#include <ctype.h>

#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "defs.h"
#include "liblog.h"
#include "libdaemon.h"
#include "onlyhtml.h"

static onlyhtml_info_t g_info = {0};

static void help();
static INT32 check_para(int argc, char **argv);

static void signal_init();
static void signal_func(int sig);

static INT32 setnonblocking(int sockfd);
static INT32 create_and_bind (UINT16 port);

static INT32 file_len(int fd);
static char *get_http_url(char *req);
static INT32 handle_request(int sockfd);
static INT32 respond_404(int sockfd);

int main(int argc, char **argv)
{
    struct epoll_event event;
    struct epoll_event *events;
    int ret, i, cnt;

    if (R_ERROR == check_para(argc, argv))
    {
        PRINTF(LEVEL_ERROR, "check_para error.\n");
        return -1;
    }
    PRINTF(LEVEL_INFORM, "check_para ok.\n");

    signal_init();


    g_info.sockfd = create_and_bind(g_info.port);
    if (R_ERROR == g_info.sockfd)
    {
        PRINTF(LEVEL_ERROR, "create_and_bind error.\n");
        return R_ERROR;
    }
    PRINTF(LEVEL_INFORM, "create_and_bind ok.\n");

    #ifdef EPOLL_CLOEXEC
      g_info.efd = epoll_create1(0);
      if (g_info.efd < 0)
    #endif
        g_info.efd = epoll_create(256);

    if (g_info.efd < 0)
    {
        PRINTF(LEVEL_ERROR, "epoll_create error.\n");
        goto _err;
    }
    PRINTF(LEVEL_INFORM, "epoll_create ok.\n");

    event.data.fd = g_info.sockfd;
    event.events = EPOLLIN;
    ret = epoll_ctl(g_info.efd, EPOLL_CTL_ADD, g_info.sockfd, &event);
    if (0 > ret)
    {
        PRINTF(LEVEL_ERROR, "epoll_ctl error [%s].\n", ERR_STR);
        goto _err;
    }
    PRINTF(LEVEL_INFORM, "epoll_ctl listen socket ok.\n");

    events = g_info.events;

    if (listen(g_info.sockfd, 10) < 0)
    {
        PRINTF(LEVEL_ERROR, "listen error!\n");
        goto _err;
    }
    PRINTF(LEVEL_INFORM, "listen ok.\n");

    g_info.state = STATE_RUNNING;
    while (STATE_STOP != g_info.state)
    {
        cnt = epoll_wait (g_info.efd, events, EVENTS_NUMBER, -1);

        for (i = 0; i < cnt; i++)
        {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN)))
            {
                // an error has occured on this fd
                PRINTF(LEVEL_DEBUG, "close fd : %d\n", events[i].data.fd);
                close(events[i].data.fd);
                continue;
            }
            else if (g_info.sockfd == events[i].data.fd)
            {
                struct sockaddr in_addr;
                socklen_t in_len;
                int infd;

                in_len = sizeof(in_addr);
                infd = accept(g_info.sockfd, &in_addr, &in_len);
                if (0 > infd)
                {
                    PRINTF(LEVEL_ERROR, "accept error.\n");
                    break;
                }

                ret = setnonblocking(infd);
                if (R_ERROR == ret)
                {
                    PRINTF(LEVEL_ERROR, "accept error.\n");
                    close(infd);
                    continue;
                }

                event.data.fd = infd;
                event.events = EPOLLIN;
                ret = epoll_ctl(g_info.efd, EPOLL_CTL_ADD, infd, &event);
                if (ret == -1)
                {
                    PRINTF(LEVEL_ERROR, "epoll_ctl error.\n");
                    close(infd);
                    continue;
                }

                PRINTF(LEVEL_DEBUG, "add a fd[%d].\n", infd);
                continue;
            }
            else
            {
                handle_request(events[i].data.fd);
            }
        }
    }

    if (0 <= g_info.sockfd) close(g_info.sockfd);
    if (0 <= g_info.efd) close(g_info.efd);
    PRINTF(LEVEL_ERROR, "exit with ok.\n");
    return 0;

_err:
    if (0 <= g_info.sockfd) close(g_info.sockfd);
    if (0 <= g_info.efd) close(g_info.efd);
    PRINTF(LEVEL_ERROR, "exit with error.\n");
    return -1;
}

static void help()
{
    printf("Usage: onlyhtml [options]\n");
    printf("Options:\n");
    printf("    -p <port>       tcp listen port, default is 80\n");
    printf("    -r <dir>        website root directory, default is /var/www/\n");
    printf("    -e <dir>        website 404 error page, default is 404.html\n");
    printf("    -d <Y|y>        run as a daemon if 'Y' or 'y', otherwise not\n");
    printf("\n");
    printf("    -l <level>      debug log level,range [0, 5]\n");
    printf("    -f <from>       debug line start number, dafault is 0\n");
    printf("    -t <to>         debug line end number, dafault is -1\n");
    printf("    -h              print help information\n");
}

static INT32 check_para(int argc, char **argv)
{
    int ch;
    INT32 bdaemon = 0;

    memset(&g_info, 0, sizeof(g_info));

    g_info.start_time = time(NULL);
    g_info.port = ONLYHTML_DEF_PORT;
    g_info.state = STATE_PREPARE;
    strncpy(g_info.root, HTTP_DEF_ROOT_DIR, HTTP_DEF_ROOT_DIR_LEN);
    strncpy(g_info.e404, HTTP_DEF_404_PAGE, HTTP_DEF_404_PAGE_LEN);

    while ((ch = getopt(argc, argv, ":d:p:l:r:e:f:t:h")) != -1)
    {
        switch (ch)
        {
            case 'd':
                if (1 == strlen(optarg) && ('Y' == optarg[0] || 'y' == optarg[0]))
                {
                    bdaemon = 1;
                }
                break;
            case 'p':
                g_info.port = atoi(optarg);
                if (0 == g_info.port)
                {
                    printf("listen port [%s] not valid.\n", optarg);
                    return R_ERROR;
                }
                break;
            case 'r':
                strncpy(g_info.root, optarg, HTTP_DEF_ROOT_DIR_LEN);
                break;
            case 'e':
                strncpy(g_info.e404, optarg, HTTP_DEF_404_PAGE_LEN);
                break;
            case 'l':
                if (0 > atoi(optarg) || 5 < atoi(optarg))
                {
                    printf("debug level [%s] out of range [0 - 5].\n", optarg);
                    return R_ERROR;
                }
                liblog_level(atoi(optarg));
                printf("log level [%d].\n", atoi(optarg));
                break;
            case 'f':
                liblog_range(atoi(optarg), liblog_range_end());
                printf("debug range [%d, %d].\n", liblog_range_start(), liblog_range_end());
                break;
            case 't':
                liblog_range(liblog_range_start(), atoi(optarg));
                printf("debug range [%d, %d].\n", liblog_range_start(), liblog_range_end());
                break;
            case 'h':
                help();
                exit(EXIT_SUCCESS);
                break;
            case '?':
                if (isprint (optopt))
                   printf("unknown option '-%c'.\n", optopt);
                else
                   printf("unknown option character '\\x%x'.\n", optopt);
                break;
            case ':':
                if (isprint (optopt))
                   printf("missing argment for '-%c'.\n", optopt);
                else
                   printf("missing argment for '\\x%x'.\n", optopt);
            default:
                break;
        }
    }

    if (bdaemon)
    {
        printf("run as a daemon.\n");
        daemonize();
    }

    chdir(g_info.root);
    
    return R_OK;
}

// signal init
static void signal_init()
{
    int sig;
 
    // ignore signal SIGPIPE
    signal(SIGPIPE, SIG_IGN);
 
    // Ctrl + C signal
    sig = SIGINT;
    if (SIG_ERR == signal(sig, signal_func))
    {
        PRINTF(LEVEL_WARNING, "%s signal[%d] failed.\n", __func__, sig);
    }
 
    // kill/pkill -15
    sig = SIGTERM;
    if (SIG_ERR == signal(sig, signal_func))
    {
        PRINTF(LEVEL_WARNING, "%s signal[%d] failed.\n", __func__, sig);
    }
}
 
// signal callback function
static void signal_func(int sig)
{
    switch(sig)
    {
        case SIGINT:
        case SIGTERM:
            PRINTF(LEVEL_INFORM, "signal [%d], exit.\n", sig);
            g_info.state = STATE_STOP;
            break;
        default:
            PRINTF(LEVEL_INFORM, "signal [%d], not supported.\n", sig);
            break;
    }
}

static INT32 setnonblocking(int sockfd)  
{  
    int opts;

    opts = fcntl(sockfd, F_GETFL);  
    if (0 > opts)
    {
        PRINTF(LEVEL_ERROR, "fcntl error.\n");
        return R_ERROR;  
    }

    opts = opts | O_NONBLOCK;
    if (0 > fcntl(sockfd, F_SETFL, opts))
    {
        PRINTF(LEVEL_ERROR, "fcntl error.\n");
        return R_ERROR;  
    }

    return R_OK;
}

static INT32 create_and_bind (UINT16 port)
{
    struct sockaddr_in serv;
    int sockfd = -1;
    int opt = 1;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        PRINTF(LEVEL_ERROR, "socket error!\n");
        return R_ERROR;
    }

    bzero((char *)&serv, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port = htons(port);
    
    if (R_ERROR == setnonblocking(sockfd))
    {
        PRINTF(LEVEL_ERROR, "setnonblocking error.\n");
        goto _err;
    }

    if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (INT8 *)&opt, sizeof(opt)))
    {
        PRINTF(LEVEL_ERROR, "setsockopt SO_REUSEADDR fail.\n");
        goto _err;
    }

    #ifdef SO_NOSIGPIPE 
    if (-1 == setsockopt(listen_sock, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof(opt)))
    {
        PRINTF(LEVEL_ERROR, "setsockopt SO_NOSIGPIPE fail.\n");
        goto _err;
    }
    #endif

    if (bind(sockfd, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        PRINTF(LEVEL_ERROR, "bind error [%d]\n", errno);
        goto _err;
    }

    return sockfd;

_err:
    if (0 <= sockfd) close(sockfd);
    return R_ERROR;
}

static ssize_t tcp_send(int sockfd, const void *buffer, size_t len, int flags)
{
    ssize_t tmp;  
    size_t total = len;  
    const char *p = buffer;  
  
    while(1)  
    {  
        tmp = send(sockfd, p, total, flags);  
        if(tmp < 0)  
        {  
            if(errno == EINTR)  
            {  
                return -1;  
            }  
 
            if(errno == EAGAIN)  
            {  
                usleep(1000);  
                continue;  
            }  
  
            return -1;  
        }  
  
        if((size_t)tmp == total)  
        {  
            return len;  
        }  
  
        total -= tmp;  
        p += tmp;  
    }  
  
    return len;  
}

static char *get_http_url(char *req)
{
    char *ptr = NULL;

    ptr = strtok(req, HTTP_HEADER_SPACE);
    if (ptr && 0 != strncmp(ptr, HTTP_REQ_GET, strlen(HTTP_REQ_GET)))
    {
        PRINTF(LEVEL_DEBUG, "not support request type [%s].\n", ptr);
        return NULL;
    }

    ptr = strtok(NULL, HTTP_HEADER_SPACE);
    PRINTF(LEVEL_DEBUG, "get url = {%s}.\n", ptr);
    if (NULL == ptr)
    {
        return NULL;
    }

    if ('\0' == ptr[1]) // if (1 == strlen(ptr))
    {
        strncpy(ptr, HTTP_DEF_INDEX, sizeof(HTTP_DEF_INDEX));
        ptr[sizeof(HTTP_DEF_INDEX)] = '\0';
        PRINTF(LEVEL_DEBUG, "default index, url = {%s}.\n", ptr);
    }
    return ptr;
}

static INT32 handle_request(int sockfd)
{
    int cnt;

    char buff[1024];
    int pos = 0;

    char *ptr = NULL;
    int fd;

    PRINTF(LEVEL_DEBUG, "request from fd[%d]: \n", sockfd);

_read:
    cnt = read(sockfd, buff + pos, 1024);
    if (0 > cnt) 
    {
        if (errno != EAGAIN)
        {
            PRINTF(LEVEL_DEBUG, "close fd[%d].\n", sockfd);
            close(sockfd);
            return R_ERROR;
        }
        else
        {
            pos += cnt;
            goto _read;
        }
    }
    else if (0 == cnt)
    {
        // client closed
        PRINTF(LEVEL_DEBUG, "close fd[%d].\n", sockfd);
        close(sockfd);
        return R_ERROR;
    }
    else if (0 < cnt)
    {
        PRINTF(LEVEL_DEBUG, "read cnt = %d from fd[%d].\n", cnt, sockfd);
    }

    ptr = get_http_url(buff);

    if (!ptr || 0 > (fd = open(ptr + 1, O_RDONLY)))
    {
        respond_404(sockfd);
        PRINTF(LEVEL_DEBUG, "close fd[%d].\n", sockfd);
        close(sockfd);
        return R_ERROR;
    }

    tcp_send(sockfd, HTTP_RESPOND_200(HTTP_FILE_IMAGE), strlen(HTTP_RESPOND_200(HTTP_FILE_IMAGE)), 0);
    pos = 0;
    while (0 < (cnt = read(fd, buff, 1024)))
    {
        pos += cnt;
        if (0 > tcp_send(sockfd, buff, cnt, 0))
        {
            PRINTF(LEVEL_DEBUG, "send error %d %s.\n", EAGAIN, ERR_STR);
        }
    }
    PRINTF(LEVEL_DEBUG, "close fd[%d], ret[%d], send[%d].\n", sockfd, cnt, pos);
    close(fd);
    close(sockfd);

    return R_OK;
}

static INT32 file_len(int fd)
{
    struct stat fdstat;

    if (-1 == fstat(fd, &fdstat))
    {
        return R_ERROR;
    }

    return fdstat.st_size;
}

static INT32 respond_404(sockfd)
{
    static int fd = -1;
    static char *buff404 = NULL;
    static int cnt;

    PRINTF(LEVEL_DEBUG, "respond_404.\n");

    if (-1 == fd)
    {
        PRINTF(LEVEL_DEBUG, "try read 404.html.\n");
        if (0 > (fd = open("./404.html", O_RDONLY)))
        {
            fd = -2;
            PRINTF(LEVEL_DEBUG, "404.html not exist.\n");
        }
        else
        {
            cnt = file_len(fd);
            if (R_ERROR == cnt)
            {
                PRINTF(LEVEL_DEBUG, "get 404.html size error.\n");
                close(fd);
                fd = -2;
            }
            else
            {
                if (NULL == (buff404 = malloc(cnt + 1)))
                {
                    PRINTF(LEVEL_DEBUG, "apply memory error.\n");
                    close(fd);
                    fd = -2;
                }
                else
                {
                    memset(buff404, 0, cnt + 1);
                    if (cnt != read(fd, buff404, cnt))
                    {
                        PRINTF(LEVEL_DEBUG, "read 404.html error.\n");
                        close(fd);
                        fd = -2;
                    }
                    else
                    {
                        close(fd);
                        fd = 0;
                        PRINTF(LEVEL_DEBUG, "read 404.html ok.\n");
                    }
                }
            }
        }
    }

    switch (fd)
    {
    case -2:
        tcp_send(sockfd, HTTP_RESPOND_404, strlen(HTTP_RESPOND_404), 0);
        break;
    default:
        tcp_send(sockfd, buff404, cnt, 0);
        break;
    }

    return R_OK;
}