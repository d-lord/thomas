#ifndef SHARED_H_
#define SHARED_H_

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

// important
typedef struct {
    int currentUsers; // network users connected right now
    pthread_mutex_t currentUsersLock;
} ProgStats;

/* code shared between user and admin space */
char* capitalise(char*, int);

#endif
