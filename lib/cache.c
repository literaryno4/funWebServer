#include <semaphore.h>
#include "../include/tlpi_hdr.h"
#include "../include/cache.h"

#define MAX_READ_NUM 65536

int inCache(char *request, struct cache *caches)
{
    int i;
    for (i = 0; i < MAX_CONN_NUM; i++) {
        if (strncmp(caches[i].request, request, strlen(request)) == 0) {
            return i;
        }
    }

    return -1;
}

char *readCache(struct cache *caches, int ind, int *readnum)
{
    if (ind < MAX_CONN_NUM) {
        sem_wait(&m);
        if (readnum[ind] >= MAX_READ_NUM) {
            readnum[ind] = 0;
        }
        readnum[ind]++;
        sem_post(&m);
        return caches[ind].object;
    }

    return NULL;
}

int writeCache(struct cache *caches, char *request, char *object, int *readnum)
{
    int i, minread = 65536;
    for (i = 0; i < MAX_CONN_NUM; i++) {
        if (readnum[i] == 0) {
            break;
        }
        if (readnum[i] < minread) {
            minread = readnum[i];
        }
    }

    sem_wait(&mutex);
    sem_wait(&m);
    readnum[i] = 1;
    strncpy(caches[i].request, request, strlen(request));
    caches[i].request[strlen(request)] = '\0';
    strncpy(caches[i].object, object, strlen(object));
    caches[i].object[strlen(object)] = '\0';
    sem_post(&m);
    sem_post(&mutex);

    return 0;
}