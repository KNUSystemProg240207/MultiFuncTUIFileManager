#ifndef __LIST_DIR_H_INCLUDED__
#define __LIST_DIR_H_INCLUDED__

#include <pthread.h>
#include <sys/stat.h>

int startDirListender(
    pthread_t *newThread,
    pthread_mutex_t *bufMutex,
    struct stat *buf,
    char (*entryNames)[MAX_NAME_LEN + 1],
    size_t bufLen,
    size_t *totalReadItems,
    pthread_cond_t *stopTrd,
    int *stopRequested,
    pthread_mutex_t *stopMutex
);

#endif
