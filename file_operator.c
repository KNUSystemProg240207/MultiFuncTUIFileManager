#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#include "file_operator.h"


struct _FileOperatorArgs {
    // 상태 관련
    uint16_t *statusFlags;
    // 진행 상황 저장
    char *opearatingFile;
    uint16_t *progress;
    // 명령 읽어올 곳
    int pipeEnd;
    // Mutex들
    pthread_mutex_t *commandMutex;
    pthread_mutex_t *statusMutex;
    // Condition Variable들
    pthread_cond_t *condResumeThread;
};
typedef struct _FileOperatorArgs FileOperatorArgs;

static FileOperatorArgs argsArr[MAX_FILE_OPERATORS];  // 각 Thread별 runtime 정보 저장
static unsigned int threadCnt = 0;  // 생성된 Thread 개수

static void *fileOperator(void *argsPtr);


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
    pthread_mutex_t *commandMutex,
    pthread_mutex_t *statusMutex,
    // Condition Variables
    pthread_cond_t *condResumeThread
) {
    if (threadCnt >= MAX_FILE_OPERATORS)
        return -1;
    
    // Do initialization
    FileOperatorArgs *newWinArg = argsArr + threadCnt;  // Thread 설정 포인터

    // 새 쓰레드 생성
    if (pthread_create(newThread, NULL, fileOperator, newWinArg) == -1) {
        return -1;
    }
    return ++threadCnt;
}

void *fileOperator(void *argsPtr) {
    FileOperatorArgs args = *(FileOperatorArgs *)argsPtr;
    struct timespec startTime, wakeupTime;
    uint64_t elapsedUSec;
    FileTask command;
    int ret;

    pthread_mutex_lock(args.statusMutex);  // 상태 보호 Mutex 획득
    *args.statusFlags = FTHREAD_FLAG_RUNNING;
    pthread_mutex_unlock(args.statusMutex);  // 상태 보호 Mutex 해제

    while (1) {
        CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &startTime));  // iteration 시작 시간 저장

        // 정지 요청 확인
        pthread_mutex_lock(args.statusMutex);  // 상태 Flag 보호 Mutex 획득
        if (*args.statusFlags & FTHREAD_FLAG_STOP) {  // 종료 요청되었으면: 중지
            *args.statusFlags = 0;
            pthread_mutex_unlock(args.statusMutex);
            goto EXIT;
        }
        pthread_mutex_unlock(args.statusMutex);  // 상태 Flag 보호 Mutex 해제

        pthread_mutex_lock(args.commandMutex);  // 결과값 보호 Mutex 획득
        read(args.pipeEnd, &command, sizeof(FileTask));
        pthread_mutex_unlock(args.commandMutex);  // 결과값 보호 Mutex 해제

        switch (command.type) {
            case COPY:
                // TODO: Implement
                break;
            case MOVE:
                // TODO: Implement
                break;
            case DELETE:
                // TODO: Implement
                break;
        }

        // 설정된 간격만큼 지연 및 종료 요청 처리
        pthread_mutex_lock(args.statusMutex);  // 상태 보호 Mutex 획득

        if (*args.statusFlags & FTHREAD_FLAG_STOP) {  // 종료 요청되었으면: 중지
            *args.statusFlags = 0;
            pthread_mutex_unlock(args.statusMutex);
            goto EXIT;
        }

        elapsedUSec = getElapsedTime(startTime);  // 실제 지연 시간 계산
        if (elapsedUSec > DIR_INTERVAL_USEC) {  // 지연 필요하면
            wakeupTime = getWakeupTime(DIR_INTERVAL_USEC - elapsedUSec);  // 재개할 '절대 시간' 계산
            ret = pthread_cond_timedwait(args.condResumeThread, args.statusMutex, &wakeupTime);  // 정지 요청 기다리며, 대기
            pthread_mutex_unlock(args.statusMutex);  // 상태 보호 Mutex 해제
            switch (ret) {
                case ETIMEDOUT:  // 대기 완료
                case EINTR:  // 재개 요청됨
                    break;  // 실행 재개
                default:
                    goto HALT;  // 오류: 쓰레드 정지
            }
        } else {
            pthread_mutex_unlock(args.statusMutex);  // 상태 보호 Mutex 해제
        }
    }
HALT:
    pthread_mutex_lock(args.statusMutex);  // 결과값 보호 Mutex 획득
    *args.statusFlags = 0;
    pthread_mutex_unlock(args.statusMutex);  // 결과값 보호 Mutex 해제
EXIT:
    return NULL;
}
