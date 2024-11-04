#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "commons.h"
#include "config.h"
#include "list_dir.h"

struct _DirListenerArgs {
    pthread_mutex_t *bufMutex;
    struct stat *buf;
    char (*nameBuf)[MAX_NAME_LEN + 1];
    size_t bufLen;
    size_t *totalReadItems;
    pthread_cond_t *stopTrd;
    int *stopRequested;
    pthread_mutex_t *stopMutex;
};
typedef struct _DirListenerArgs DirListenerArgs;

static DirListenerArgs argsArr[MAX_DIRWINS];
static unsigned int subWinCnt = 0;

static void *dirListener(void *);
static ssize_t listEntries(struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen);
static struct timespec getWakeupTime(uint32_t wakeupUs);


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
) {
    argsArr[subWinCnt] = (DirListenerArgs) {
        .bufMutex = bufMutex,
        .buf = buf,
        .nameBuf = entryNames,
        .bufLen = bufLen,
        .totalReadItems = totalReadItems,
        .stopTrd = stopTrd,
        .stopRequested = stopRequested,
        .stopMutex = stopMutex,
    };
    if (pthread_create(newThread, NULL, dirListener, argsArr + subWinCnt) == -1) {
        return -1;
    }
    subWinCnt++;
    return subWinCnt;
}

void *dirListener(void *argsPtr) {
    DirListenerArgs args = *(DirListenerArgs *)argsPtr;
    struct timespec startTime, wakeupTime;
    uint64_t elapsedUSec;
    size_t readItems;
    int ret;
    while (1) {
        CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &startTime));

        // Refresh directory
        pthread_mutex_lock(args.bufMutex);
        readItems = listEntries(args.buf, args.nameBuf, args.bufLen);
        CHECK_FAIL(readItems);
        *args.totalReadItems = readItems;
        pthread_mutex_unlock(args.bufMutex);

        // Delay
        elapsedUSec = getElapsedTime(startTime);
        pthread_mutex_lock(args.stopMutex);
        if (*args.stopRequested) {
            pthread_mutex_unlock(args.stopMutex);
            break;
        }
        elapsedUSec = getElapsedTime(startTime);
        if (elapsedUSec > DIR_INTERVAL_USEC) {
            wakeupTime = getWakeupTime(DIR_INTERVAL_USEC - elapsedUSec);
            ret = pthread_cond_timedwait(args.stopTrd, args.stopMutex, &wakeupTime);
            switch (ret) {
                case ETIMEDOUT:
                    break;
                case EINTR:
                    pthread_mutex_unlock(args.stopMutex);
                    goto EXIT;
            }
        }
        pthread_mutex_unlock(args.stopMutex);
    }
EXIT:
    return NULL;
}

ssize_t listEntries(struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen) {
    // TODO: dir null check
    DIR *dir = opendir(".");
    if (dir == NULL) {
        return -1;
    }
    size_t readItems = 0;
    errno = 0;
    for (struct dirent *ent = readdir(dir); readItems < bufLen; ent = readdir(dir)) {
        if (ent == NULL) {
            if (errno != 0) {
                return -1;
            }
            return readItems;
        }
        // if (
        //     (ent->d_name[0] == '.' && ent->d_name[1] == '.' && ent->d_name[2] == '\0')
        //     || (ent->d_name[0] == '.' && ent->d_name[1] == '\0')
        // ) {
        //     continue;
        // }
        strncpy(nameBuf[readItems], ent->d_name, MAX_NAME_LEN);
        nameBuf[readItems][MAX_NAME_LEN] = '\0';
        if (stat(ent->d_name, resultBuf + readItems) == -1) {
            return -1;
        }
        errno = 0;
        readItems++;
    }
    return readItems;
}

struct timespec getWakeupTime(uint32_t wakeupUs) {
    struct timespec now;
    CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &now));
    uint64_t newNsec = now.tv_nsec + wakeupUs * 1000;
    now.tv_sec += newNsec / (1000 * 1000 * 1000);
    now.tv_nsec = newNsec % (1000 * 1000 * 1000);
    return now;
}
