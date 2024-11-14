#ifndef __FILE_OPERATOR_H_INCLUDED__
#define __FILE_OPERATOR_H_INCLUDED__

#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>

#include "config.h"

#define FTHREAD_FLAG_RUNNING (1 << 0)  // 쓰레드 실행 여부
#define FTHREAD_FLAG_STOP (1 << 8)  // 쓰레드 정지 요청

typedef enum _FileOpration {
    COPY,
    MOVE,
    DELETE
} FileOpration;

typedef struct _FileTask {
    FileOpration type;
    int srcDirFd;
    dev_t srcDevNo;
    char srcName[MAX_NAME_LEN];
    size_t fileSize;
    int dstDirFd;
    dev_t dstDevNo;
    char dstName[MAX_NAME_LEN];
} FileTask;

/**
 * 새 File Operator Thread 시작
 *
 * @param newThread (반환) 새 쓰레드
 * @param statusFlags (반환) 새 쓰레드의 Status Flags
 * @return 성공: 0, 실패: -1
 */
int startFileOperator(
    // 반환값들
    pthread_t *newThread,
    // 상태 관련
    uint16_t *statusFlags,
    // 결과 저장
    char *currentOperation,
    // 명령 읽어올 곳
    int pipeEnd,
    // Mutexes
    pthread_mutex_t *operations,
    pthread_mutex_t *statusMutex,
    // Condition Variables
    pthread_cond_t *condResumeThread
);

#endif
