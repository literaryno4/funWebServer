#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "../include/tlpi_hdr.h"

static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_key_t strerrorKey;

#define MAX_ERROR_LEN 256

static void
destructor(void *buf)
{
    free(buf);
}

static void
createKey(void)
{
    int s;
    s = pthread_key_create(&strerrorKey, destructor);
    if (s != 0) {
        errExitEN(s, "pthread_key_create");
    }
}

char *
strerror(int err)
{
    int s;
    char *buf;

    s = pthread_once(&once, createKey);
    if (s != 0) {
        errExitEN(s, "pthread_once");
    }

    buf = pthread_getspecific(strerrorKey);
    if (buf == NULL) {
        buf = malloc(MAX_ERROR_LEN);
        if (buf == NULL) {
            errExit("malloc");
        }

        s = pthread_setspecific(strerrorKey, buf);
        if (s != 0) {
            errExitEN(s, "pthread_setspecific");
        }
    }

    if (err < 0 || err >= _sys_nerr || _sys_errlist[err] == NULL) {
        snprintf(buf, MAX_ERROR_LEN, "unknown error %d", err);
    } else {
        strncpy(buf, _sys_errlist[err], MAX_ERROR_LEN - 1);
        buf[MAX_ERROR_LEN - 1] = '\0';
    }

    return buf;
}