#ifndef PROC_WIN_H
#define PROC_WIN_H

#include <pthread.h>
#include <sys/types.h>

int initProcessWindow(pthread_mutex_t *_bufMutex, size_t *_totalReadItems, Process *_processes);
void hideProcessWindow();
void delProcessWindow();
void updateProcessWindow();

void selectPreviousProcess();
void selectNextProcess();
void getSelectedProcess(pid_t *pid, char *nameBuf, size_t bufLen);

#endif
