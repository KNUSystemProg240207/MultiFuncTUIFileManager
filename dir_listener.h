#ifndef __DIR_LISTENER_H_INCLUDED__
#define __DIR_LISTENER_H_INCLUDED__

#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>

#define LTHREAD_FLAG_RUNNING (1 << 0)  // 쓰레드 실행 여부
#define LTHREAD_FLAG_STOP (1 << 8)  // 쓰레드 정지 요청
#define LTHREAD_FLAG_CHANGE_DIR (1 << 2)  // 쓰레드 디렉터리 변경 요청

/**
 * 새 Directory Listener Thread 시작
 *
 * @param newThread (반환) 새 쓰레드
 * @param statusFlags 새 쓰레드의 Status Flags
 * @param chdirIdx 이동할 Directory의 index (statusFlags |= LTHREAD_FLAG_CHANGE_DIR 하여 사용)
 * @param statBuf Data Buffer: Stat 결과 저장
 * @param entryNames Data Buffer: 하위 항목 이름 저장
 * @param totalReadItems 읽어들인 항목 수
 * @param bufLen 두 Buffer의 길이
 * @param bufMutex Data Buffer Mutex
 * @param statusMutex 상태 (Flag 및 재개 알림) Mutex
 * @param condResumeThread 새 쓰레드의 재개 요청
 * @return 성공: 0, 실패: -1
 */
int startDirListender(
    // 반환값들
    pthread_t *newThread,
    // 상태 관련
    uint16_t *statusFlags,
    size_t *chdirIdx,
    // 결과 Buffer
    struct stat *statBuf,
    char (*entryNames)[MAX_NAME_LEN + 1],
    size_t *totalReadItems,
    size_t bufLen,
    // Mutexes
    pthread_mutex_t *bufMutex,
    pthread_mutex_t *statusMutex,
    // Condition Variables
    pthread_cond_t *condResumeThread
);

#endif
