#ifndef _THREAD_COMMONS_H_INCLUDED_
#define _THREAD_COMMONS_H_INCLUDED_

#include <pthread.h>
#include <stdint.h>


#define THREAD_FLAG_RUNNING (1 << 0)  // Thread 실행 여부
#define THREAD_FLAG_STOP (1 << 1)  // Thread 정지 요청
#define THREAD_FLAG_PAUSE (1 << 2)  // Thread 일시정지 요청
#define THREAD_FLAG_MSB 2  // thread_commons에서 사용하는 가장 큰 bit


/**
 * @struct _DirListenerArgs
 *
 * @var _ThreadArgs::statusFlags 상태 Flag
 * @var _ThreadArgs::statusMutex Flag 및 재개 알림 보호 Mutex
 * @var _ThreadArgs::condResumeThread 쓰레드 재개 필요 알림 Condition Variable
 */
typedef struct _ThreadArgs {
    uint16_t statusFlags;  // 상태 Flag
    pthread_mutex_t statusMutex;  // Flag 및 재개 알림 보호 Mutex
    pthread_cond_t resumeThread;  // 쓰레드 재개 필요 알림 Condition Variable
} ThreadArgs;


/**
 * 새 Thread 시작
 *`
 * @param newThread (반환) 새 쓰레드
 * @param onInit Thread의 Main Loop 진입 직전 실행될 함수 (NULL = 호출할 함수 없음)
 * @param loop Thread의 Main Loop에서, Loop 1회 실행 될때마다 호출될 함수
 * @param onFinish Thread의 Main Loop 종료 직후 실행될 함수 (NULL = 호출할 함수 없음)
 * @param loopInterval 1회 반복 간 간격 (다음 시작 시간 - 이전 시작 시간) [단위: μs]
 * @param threadArgs 새 Thread의 공유 변수
 * @param targetFuncArgs 'onInit', 'loop', 'onFinish' 3개의 각 함수에 전달할 인자 (참고: 구조체 형태로 전달)
 * @return 성공: 0, 실패: (오류 코드: pthread_create 참조)
 */
int startThread(
    pthread_t *newThread,
    int (*onInit)(void *),
    int (*loop)(void *),
    int (*onFinish)(void *),
    uint64_t loopInterval,
    ThreadArgs *threadArgs,
    void *targetFuncArgs
);

/**
 * Thread 정지 요쳥 (주의: 즉시 정지되지 않으며, iteration 끝난 후 정지됨)
 *
 * @param threadArgs 정지시키려는 Thread의 ThreadAargs 구조체
 */
int stopThread(ThreadArgs *args);

/**
 * Thread 일시 정지 요쳥 (주의: 즉시 일시정지되지 않으며, iteration 끝난 후 정지됨)
 *
 * @param threadArgs 일시정지시키려는 Thread의 ThreadAargs 구조체
 */
int pauseThread(ThreadArgs *args);

/**
 * Thread 재개
 *
 * @param threadArgs 일시정지시키려는 Thread의 ThreadAargs 구조체
 */
int resumeThread(ThreadArgs *args);

#endif
