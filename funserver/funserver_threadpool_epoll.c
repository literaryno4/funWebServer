#include "../include/inet_sockets.h"
#include "../include/read_line.h"
#include "../include/sbuf.h"
#include "../include/tlpi_hdr.h"
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>

#define PORT_NUM "50004"
#define WORKDIR "."
#define BACKLOG 50
#define BUF_SIZE 1000
#define MAX_FILE_SIZE 100000
#define MAX_THREAD_NUMBER 4
#define READ_BUF_SIZE 2048
#define MAX_FD 65536
#define MAX_EVENTS 1000

sbuf_t sp;

struct users
{
    char* filename;
    int read_buf_len;
    char u_read_buf[READ_BUF_SIZE];
};

struct users usrs[MAX_FD];

void errPage(int cfd, char* cause, char* errnum, char* shortmsg, char* longmsg);

void readothhrd(int cfd);

void getFiletype(char* filename, char* filetype);

int parseUrl(char* url, char* filename, char* cgiargs);

int staticRequest(int cfd, char* filename, int filesize);

int dynamicRequest(int cfd, char* filename, char* cgiargs);

void modfd(int epollfd, int fd, int ev);

int setnonblocking(int fd);

int serveWebRead(int cfd);

int serveWebWrite(int cfd);

static void* worker(struct users* usrs);

int
main(int argc, char* argv[])
{
    int lfd, cfd, s;
    pthread_t t1;
    struct sockaddr_storage claddr;
    socklen_t addrlen;
    struct sigaction sa;

    if (daemon(0, 0) == 1) {
        errExit("daemon");
    }

    sbuf_init(&sp, 16);

    // create thread pool
    for (int i = 0; i < MAX_THREAD_NUMBER; i++) {
        s = pthread_create(&t1, NULL, worker, usrs);
        if (s != 0) {
            syslog(LOG_ERR, "pthread_create error: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < MAX_FD; i++) {
        memset(usrs[i].u_read_buf, '\0', READ_BUF_SIZE);
    }

    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = SA_RESTART;
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        syslog(LOG_ERR, "sigaciton error: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    lfd = inetListen(PORT_NUM, BACKLOG, addrlen);

    for (;;) {
        cfd = accept(lfd, (struct sockaddr*)&claddr, &addrlen);
        if (cfd == -1) {
            continue;
        }

        syslog(LOG_INFO, "socket connected");

        sbuf_insert(&sp, cfd);
    }
}

static void*
worker(struct users* usrs)
{
    pthread_detach(pthread_self());

    int cfd, epollFd, numEvents, i;
    struct epoll_event ev;
    struct epoll_event evlist[MAX_EVENTS];
    epollFd = epoll_create(5);
    if (epollFd == -1) {
        syslog(LOG_ERR, "epoll_create error: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    for (;;) {
        cfd = sbuf_tryremove(&sp);   // try sem_wait
        if (cfd >= 0) {
            ev.data.fd = cfd;
            ev.events  = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLERR;
            epoll_ctl(epollFd, EPOLL_CTL_ADD, cfd, &ev);
            setnonblocking(cfd);
        }

        numEvents = epoll_wait(epollFd, evlist, MAX_EVENTS, 0);

        for (i = 0; i < numEvents; i++) {
            if (evlist[i].events & EPOLLERR) {
                close(evlist[i].data.fd);
                continue;
            } else if (evlist[i].events & EPOLLIN) {
                serveWebRead(evlist[i].data.fd);
                modfd(epollFd, evlist[i].data.fd, EPOLLOUT);
            } else if (evlist[i].events & EPOLLOUT) {
                serveWebWrite(evlist[i].data.fd);
                modfd(epollFd, evlist[i].data.fd, EPOLLIN);
            } else {
                close(evlist[i].data.fd);
                continue;
            }
        }
    }
}

int
serveWebRead(int cfd)
{
    int numread;

    if ((numread = readLine(cfd, usrs[cfd].u_read_buf, READ_BUF_SIZE)) == -1) {
        // close(cfd);
        return -1;
    } else if (numread == 0) {
        // close(cfd);
        return -1;
    }
    readothhrd(cfd);

    return 0;
}

int
serveWebWrite(int cfd)
{
    int isStatic;
    struct stat statbuf;
    char method[BUF_SIZE], url[BUF_SIZE], version[BUF_SIZE], filename[BUF_SIZE],
        cgiargs[BUF_SIZE];

    if (strlen(usrs[cfd].u_read_buf) == 0) {
        close(cfd);
        return -1;
    }
    syslog(LOG_INFO, "%s\n", usrs[cfd].u_read_buf);
    sscanf(usrs[cfd].u_read_buf, "%s %s %s", method, url, version);
    memset(usrs[cfd].u_read_buf, '\0', READ_BUF_SIZE);

    if (strcmp(method, "GET")) {
        errPage(cfd,
                method,
                "501",
                "Not Implemented",
                "funserver does not implement this method");
        close(cfd);

        return -1;
    }

    isStatic = parseUrl(url, filename, cgiargs);

    if (stat(filename, &statbuf) == -1) {
        errPage(cfd,
                filename,
                "404",
                "Not found",
                "funserver couldn't find this file");
        close(cfd);
        return -1;
    }
    if (isStatic) {
        staticRequest(cfd, filename, statbuf.st_size);
    } else {
        dynamicRequest(cfd, filename, cgiargs);
    }

    return 0;
}

int
setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void
modfd(int epollfd, int fd, int ev)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events  = ev | EPOLLET | EPOLLERR;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void
errPage(int cfd, char* cause, char* errnum, char* shortmsg, char* longmsg)
{
    char buf[BUF_SIZE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    write(cfd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    write(cfd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    write(cfd, buf, strlen(buf));
    sprintf(buf,
            "<body bgcolor="
            "ffffff"
            ">\r\n");
    write(cfd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    write(cfd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    write(cfd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Fun Web server</em>\r\n\r\n");
    write(cfd, buf, strlen(buf));
}

void
readothhrd(int cfd)
{
    char buf[BUF_SIZE];
    readLine(cfd, buf, BUF_SIZE);
    while (strcmp(buf, "\r\n")) {
        readLine(cfd, buf, BUF_SIZE);
        syslog(LOG_INFO, "%s", buf);
    }
    return;
}

void
getFiletype(char* filename, char* filetype)
{
    if (strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    } else if (strstr(filename, ".gif")) {
        strcpy(filetype, "text/gif");
    } else if (strstr(filename, ".png")) {
        strcpy(filetype, "text/png");
    } else if (strstr(filename, ".jpg")) {
        strcpy(filetype, "text/jpeg");
    } else if (strstr(filename, ".css")) {
        strcpy(filetype, "text/css");
    } else if (strstr(filename, ".js")) {
        strcpy(filetype, "text/js");
    } else {
        strcpy(filetype, "text/plain");
    }
}

int
parseUrl(char* url, char* filename, char* cgiargs)
{
    char* ptr;

    if (!strstr(url, "cgi-bin")) {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, url);
        if (url[strlen(url) - 1] == '/') {
            strcat(filename, "index.html");
        }

        return 1;
    } else {
        ptr = index(url, '?');
        if (ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        } else {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, url);

        return 0;
    }
}

int
staticRequest(int cfd, char* filename, int filesize)
{
    int n, fd;
    char buf[BUF_SIZE], filetype[BUF_SIZE];
    getFiletype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    write(cfd, buf, strlen(buf));
    sprintf(buf, "Server: Fun Web Server\r\n");
    write(cfd, buf, strlen(buf));
    sprintf(buf, "Connection close\r\n");
    write(cfd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    write(cfd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    write(cfd, buf, strlen(buf));

    if ((fd = open(filename, O_RDONLY)) == -1) {
        return -1;
    }

    while ((n = sendfile(cfd, fd, NULL, filesize)) > 0)
        ;
    if (n == -1) {
        close(fd);
        return -1;
    }
    close(fd);
    // fdp = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    // close(fd);
    // n = write(cfd, fdp, filesize);
    // if (n != filesize) {
    //     printf("error");
    //     return -1;
    // }
    // munmap(fdp, filesize);

    return 0;
}

int
dynamicRequest(int cfd, char* filename, char* cgiargs)
{
    dup2(cfd, STDOUT_FILENO);
    execve(filename, NULL, NULL);
    return 0;
}
