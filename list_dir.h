#ifndef __LIST_DIR_H_INCLUDED__
#define __LIST_DIR_H_INCLUDED__

#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * 새 Directory Listener Thread 시작
 *
 * @param newThread (반환) 새 쓰레드
 * @param bufMutex Data Buffer Mutex
 * @param buf Data Buffer: Stat 결과 저장
 * @param entryNames Data Buffer: 하위 항목 이름 저장
 * @param bufLen 두 Buffer의 길이
 * @param totalReadItems (반환) 읽어들인 항목 수
 * @param stopTrd 생성된 Thread 정지 요청하는 Condition Variable
 * @param stopRequested 생성된 Thread 정지 요청 (Condition Variable을 Wait하기 전, 정지 요청 요청 처리용) (true: 요청됨)
 * @param stopMutex 정지 요청 관련 Mutex
 * @return 성공: 0, 실패: -1
 */
int startDirListender(
    pthread_t *newThread,
    pthread_mutex_t *bufMutex,
    struct stat *buf,
    char (*entryNames)[MAX_NAME_LEN + 1],
    size_t bufLen,
    size_t *totalReadItems,
    pthread_cond_t *stopTrd,
    bool *stopRequested,
    pthread_mutex_t *stopMutex
);

typedef enum {
    JOB_NONE,
    JOB_COPY,
    JOB_MOVE,
    JOB_REMOVE,
    JOB_ENTER
} JobType;
extern JobType currentJob;
extern pthread_mutex_t jobMutex;
extern pthread_cond_t jobCond;
extern char curSelectedName[MAX_NAME_LEN + 1];

void performCopy();
void performMove();
void performRemove();
void performEnter();

#endif
