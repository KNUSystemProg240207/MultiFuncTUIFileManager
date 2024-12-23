#ifndef _PROC_THREAD_H_INCLUDED_
#define _PROC_THREAD_H_INCLUDED_

#include <pthread.h>
#include <stddef.h>

#include "thread_commons.h"

typedef struct _Process {
    pid_t pid;  // 프로세스 ID
    char name[NAME_MAX + 1];  // 프로세스 이름
    char state;  // 프로세스 상태
    long rsize;  // '실제 메모리 점유량 (= rss * pageSize)
    unsigned long utime;  // 사용자 모드에서의 CPU 시간
    unsigned long stime;  // 커널 모드에서의 CPU 시간
} Process;

/**
 * @struct ProcessThreadArgs
 *
 * @var _ProcThreadArgs::commonArgs 스레드 상태 및 재개 정보
 * @var _ProcThreadArgs::entriesMutex processEntries 보호 Mutex
 * @var _ProcThreadArgs::processEntries 프로세스 정보 배열
 * @var _ProcThreadArgs::totalReadItems 읽어들인 총 프로세스 수
 */
typedef struct {
    ThreadArgs commonArgs;  // 스레드 상태 및 재개 정보
    pthread_mutex_t entriesMutex;  // processEntries 보호 Mutex
    Process processEntries[MAX_PROCESSES];  // 프로세스 정보 배열
    size_t totalReadItems;  // 읽어들인 총 프로세스 수
} ProcessThreadArgs;


/**
 * 프로세스 정보 스레드 시작 함수
 *
 * @param newThread 새로 생성된 쓰레드 반환
 * @param args 스레드에 전달할 ProcessThreadArgs 구조체
 * @return 성공: 0, 실패: -1
 */
int startProcessThread(pthread_t *newThread, ProcessThreadArgs *args);

#endif