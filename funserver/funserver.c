#include <sys/socket.h>
#include <netdb.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <sys/mman.h>
#include "../include/tlpi_hdr.h"
#include "../include/read_line.h"

#define PORT_NUM "50004"
#define BACKLOG 50
#define BUF_SIZE 1000
#define MAX_FILE_SIZE 100000

void 
errPage(int cfd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
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
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    write(cfd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    write(cfd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    write(cfd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Fun Web server</em>\r\n");
    write(cfd, buf, strlen(buf));
}

void
readothhrd(int cfd)
{
    int numread;
    char buf[BUF_SIZE];
    readLine(cfd, buf, BUF_SIZE);
    while(strcmp(buf, "\r\n")) {
        readLine(cfd, buf, BUF_SIZE);
        printf("%s", buf);
    }
    return;
}

void 
getFiletype(char *filename, char* filetype)
{
    if (strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    } else if (strstr(filename, ".gif")) {
        strcpy(filetype, "text/gif");
    } else if (strstr(filename, ".png")) {
        strcpy(filetype, "text/png");
    } else if (strstr(filename, ".jpg")) {
        strcpy(filetype, "text/jpeg");
    } else {
        strcpy(filetype, "text/plain");
    }
}

int
parseUrl(char *url, char *filename, char* cgiargs)
{
    char *ptr;
    
    if (!strstr(url, "cgi-bin")) {
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, url);
        if (url[strlen(url)-1] == '/') {
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
staticRequest(int cfd, char *filename, int filesize)
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

    while((n = sendfile(cfd, fd, NULL, filesize)) > 0);
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
    printf("sent file\n");

    return 0;
}

int
dynamicRequest(int cfd, char *filename, char *cgiargs)
{
    dup2(cfd, STDOUT_FILENO);
    execve(filename, NULL, NULL);
    return 0;
}

int
serveWeb(int cfd)
{
    int numread, isStatic;
    struct stat sbuf;
    char buf[BUF_SIZE], method[BUF_SIZE], url[BUF_SIZE], 
            version[BUF_SIZE], filename[BUF_SIZE], cgiargs[BUF_SIZE];
    
    if ((numread = readLine(cfd, buf, BUF_SIZE)) == -1) {
        return -1;
    }
    sscanf(buf, "%s %s %s", method, url, version);
    readothhrd(cfd);

    if (strcmp(method, "GET")) {
        errPage(cfd, method, "501", "Not Implemented",
                    "funserver does not implement this method");

        return -1;
    }

    isStatic = parseUrl(url, filename, cgiargs);

    if (stat(filename, &sbuf) == -1) {
        errPage(cfd, filename, "404", "Not found",
            "funserver couldn't find this file");
        return -1;
    }
    if (isStatic) {
        staticRequest(cfd, filename, sbuf.st_size);
    } else {
        dynamicRequest(cfd, filename, cgiargs);

    }

    printf("received GET req\n");

    return 0;
}

int
main(int argc, char *argv[])
{
    int lfd, cfd, optval, s;
    struct sockaddr_storage claddr;
    socklen_t addrlen;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        errExit("sigaction");
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_next = NULL;
    hints.ai_addr = NULL;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;

    if (getaddrinfo(NULL, PORT_NUM, &hints, &result) == -1) {
        errExit("getaddrinfo");
    }

    optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        lfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (lfd == -1) {
            continue;
        }

        if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) 
               == -1) {
            errExit("setsockopt");
        }

        if (bind(lfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(lfd);
    }

    if (rp == NULL) {
        fatal("could not bind");
    }

    if (listen(lfd, BACKLOG) == -1) {
        errExit("listen");
    }

    freeaddrinfo(result);

    for (;;) {
        cfd = accept(lfd, (struct sockaddr *) &claddr, &addrlen);
        if (cfd == -1) {
            continue;
        }

        printf("connected\n");
        
        switch (fork()) {
        case -1:
            printf("error fork");
            close(cfd);
            break;
        case 0:
            close(lfd);
            serveWeb(cfd);
            _exit(EXIT_SUCCESS);
        default:
            close(cfd);
            break;
        }
    }
}