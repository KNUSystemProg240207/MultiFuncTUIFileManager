#ifndef __PROC_THREAD_H_INCLUDED__
#define __PROC_THREAD_H_INCLUDED__

#include <pthread.h>
#include "thread_commons.h"
#include "proc_win.h"

/**
 * @struct ProcThreadArgs
 * 
 * @var threadArgs ThreadArgs 구조체로 스레드 상태와 관련된 정보를 포함
 * @var procWin 프로세스 창에 대한 정보를 저장
 */
typedef struct {
    ThreadArgs threadArgs;  // 스레드 상태 및 재개 정보
    ProcWin *procWin;       // 프로세스 창 정보
	pthread_mutex_t *procWinMutex;
} ProcThreadArgs;



/**
 * 프로세스 정보 스레드 시작 함수
 *
 * @param newThread 새로 생성된 쓰레드 반환
 * @param args 스레드에 전달할 ProcThreadArgs 구조체
 * @return 성공: 0, 실패: -1
 */
int startProcThread(pthread_t *newThread, ProcThreadArgs *args);
int procThreadMain(void *argsPtr);
int readProcInfo(ProcWin *procWindow);
size_t findInsertPosition(ProcInfo *pointerArray[], size_t size, unsigned long vsize);

#endif