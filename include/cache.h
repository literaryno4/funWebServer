#ifndef __CACHE_H__
#define __CACHE_H__

#include <semaphore.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define BUF_SIZE 204
#define MAX_CONN_NUM 10

struct cache {
    char request[BUF_SIZE];
    char object[MAX_CACHE_SIZE];
};

sem_t mutex, m;

int inCache(char *request, struct cache * caches);

char *readCache(struct cache * caches, int ind, int *readnum);

int writeCache(struct cache *caches, char *request, char *object, int *readnum);

#endif