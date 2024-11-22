#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "commons.h"
#include "config.h"
#include "thread_commons.h"


typedef struct _RunnerArgument {
    int (*onInit)(void *);
    int (*loop)(void *);
    int (*onFinish)(void *);
    uint64_t loopInterval;
    ThreadArgs *threadArgs;
    void *targetFuncArgs;
} RunnerArgument;


/**
 * Thread가 실행시킬 함수
 *
 * @param runnerArgument RunnerArgument 구조체의 포인터 (주의: automatic variable (local variable 등) 전달 금지!)
 * @return 없음
 */
static void *runner(void *runnerArgument);

/**
 * 깨어날 시간 계산
 *
 * @param wakeupUs 대기 시간
 * @return 깨어날 시간 (Clock: CLOCK_REALTIME 기준)
 */
static struct timespec getWakeupTime(uint32_t wakeupUs);


int startThread(
    pthread_t *newThread,
    int (*onInit)(void *),
    int (*loop)(void *),
    int (*onFinish)(void *),
    uint64_t loopInterval,
    ThreadArgs *threadArgs,
    void *targetFuncArgs
) {
    RunnerArgument *argument = malloc(sizeof(RunnerArgument));
    assert(argument != NULL);

    argument->onInit = onInit;
    argument->loop = loop;
    argument->onFinish = onFinish;
    argument->loopInterval = loopInterval;
    argument->threadArgs = threadArgs;
    argument->targetFuncArgs = targetFuncArgs;

    return pthread_create(newThread, NULL, runner, argument);
}

void *runner(void *runnerArgument) {
    RunnerArgument *argument = (RunnerArgument *)runnerArgument;  // Cast
    // Thread의 runtime information 저장
    int (*onInit)(void *) = argument->onInit;
    int (*loop)(void *) = argument->loop;
    int (*onFinish)(void *) = argument->onFinish;
    uint64_t loopInterval = argument->loopInterval;
    ThreadArgs *threadArgs = argument->threadArgs;
    void *targetFuncArgs = argument->targetFuncArgs;
    free(runnerArgument);  // 메모리 해제

    assert(loop != NULL);

    // (필요하면) onInit 함수 호출
    if (onInit != NULL)
        onInit(targetFuncArgs);

    // 'Thread 작동 중' Flag 설정
    pthread_mutex_lock(&threadArgs->statusMutex);  // 상태 보호 Mutex 획득
    threadArgs->statusFlags = THREAD_FLAG_RUNNING;
    pthread_mutex_unlock(&threadArgs->statusMutex);  // 상태 보호 Mutex 해제

    // Main Loop
    struct timespec startTime;  // Loop 시작 시간
    struct timespec wakeupTime;  // 다시 깨어날 시간
    uint64_t elapsedUSec;  // 이 Iteration에서 흐른 시간 [단위: μs]
    int ret;  // 각종 함수 Return값 (임시 변수)
    while (1) {
        CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &startTime));  // iteration 시작 시간 저장

        // loop 함수 실행 전 정지 요청 확인
        pthread_mutex_lock(&threadArgs->statusMutex);  // 상태 Flag 보호 Mutex 획득
        if (threadArgs->statusFlags & THREAD_FLAG_STOP) {  // 종료 요청되었으면: 중지
            goto EXIT_LOOP;
        }
        pthread_mutex_unlock(&threadArgs->statusMutex);  // 상태 Flag 보호 Mutex 해제

        loop(targetFuncArgs);  // Thread loop 함수 호출

        // 설정된 간격만큼 지연 및 종료 요청 처리
        pthread_mutex_lock(&threadArgs->statusMutex);  // 상태 보호 Mutex 획득

        // 지연 전 종료 확인
        if (threadArgs->statusFlags & THREAD_FLAG_STOP) {  // 종료 요청되었으면: 중지
            goto EXIT_LOOP;
        }

        elapsedUSec = getElapsedTime(startTime);  // 실제 지연 시간 계산
        if (elapsedUSec > loopInterval) {  // 지연 필요하면
            wakeupTime = getWakeupTime(loopInterval - elapsedUSec);  // 재개할 '절대 시간' 계산
            ret = pthread_cond_timedwait(&threadArgs->resumeThread, &threadArgs->statusMutex, &wakeupTime);  // 정지 요청 기다리며, 대기
            // 주의: 현재 statusMutex 획득된 상태 -> 코드 작성 시 유의
            switch (ret) {
                case 0:  // 재개 요청됨
                case ETIMEDOUT:  // 대기 완료
                case EINTR:  // 대기 중 Interrupt 발생
                    pthread_mutex_unlock(&threadArgs->statusMutex);  // 상태 보호 Mutex 해제
                    break;  // 실행 재개
                default:
                    goto HALT;  // 오류: 쓰레드 정지
            }
        } else {
            pthread_mutex_unlock(&threadArgs->statusMutex);  // 상태 보호 Mutex 해제
        }
    }

EXIT_LOOP:
    // (필요하면) onFinish 함수 호출
    if (onFinish != NULL) {
        // 상태 보호 Mutex 해제:
        // 항상 Loop 내부에서 획득된 상태로 빠져나오며,
        // onFinish 함수 오래 걸릴 수도 있음
        pthread_mutex_unlock(&threadArgs->statusMutex);
        onFinish(targetFuncArgs);
        // 상태 보호 Mutex 다시 획득: statusFlags 초기화 위해
        pthread_mutex_lock(&threadArgs->statusMutex);
    }

HALT:
    // Thread Flag 초기화 (정지된 것으로 Marking 포함)
    // 상태 보호 Mutex 획득 불필요:
    // 획득된 상태로 Loop 빠져나옴 or 윗쪽 코드에서 이미 획득
    // pthread_mutex_lock(&threadArgs->statusMutex);
    threadArgs->statusFlags = 0;
    // 상태 보호 Mutex 해제
    pthread_mutex_unlock(&threadArgs->statusMutex);

    return NULL;
}

struct timespec getWakeupTime(uint32_t wakeupUs) {
    struct timespec time;
    CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &time));  // 현재 시간 가져옴
    uint64_t newNsec = time.tv_nsec + wakeupUs * 1000;  // 현재 시간 + 지연 시간 계산
    time.tv_sec += newNsec / (1000 * 1000 * 1000);  // 깨어날 초 설정
    time.tv_nsec = newNsec % (1000 * 1000 * 1000);  // 깨어날 나노초 설정
    return time;
}
