#ifndef __DIR_WINDOW_H_INCLUDED__
#define __DIR_WINDOW_H_INCLUDED__

#include <curses.h>
#include <pthread.h>
#include <sys/stat.h>

int initDirWin(
    pthread_mutex_t *statMutex,
    struct stat *statEntries,
    char **entryNames,
    size_t *totalReadItems
);

int updateWins(void);

#endif
