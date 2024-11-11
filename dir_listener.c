#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "commons.h"
#include "config.h"
#include "dir_listener.h"


/**
 * @struct _DirListenerArgs
 *
 * @var _DirListenerArgs::statBuf 항목들의 stat 결과 저장 공간
 * @var _DirListenerArgs::nameBuf 항목들의 이름 저장 공간
 * @var _DirListenerArgs::totalReadItems 총 읽어들인 개수 저장 변수
 * @var _DirListenerArgs::bufLen 두 buffer들의 길이
 * @var _DirListenerArgs::stopRequested 정지 여부 확인 변수
 * @var _DirListenerArgs::bufMutex 두 결과값 buffer 보호 Mutex
 * @var _DirListenerArgs::statusMutex Flag 및 재개 알림 보호 Mutex
 * @var _DirListenerArgs::condResumeThread 쓰레드 재개 필요 알림 Condition Variable
 */
struct _DirListenerArgs {
    // 상태 관련
    uint16_t *statusFlags;
    size_t *chdirIdx;
    DIR *currentDir;
    // 결과 Buffer
    struct stat *statBuf;
    char (*nameBuf)[MAX_NAME_LEN + 1];
    size_t *totalReadItems;
    size_t bufLen;
    // Mutexes
    pthread_mutex_t *bufMutex;
    pthread_mutex_t *statusMutex;
    // Condition Variables
    pthread_cond_t *condResumeThread;
};
typedef struct _DirListenerArgs DirListenerArgs;

static DirListenerArgs argsArr[MAX_DIRWINS];  // 각 Thread별 runtime 정보 저장
static unsigned int threadCnt = 0;  // 생성된 Thread 개수

/**
 * (Thread의 loop 함수) 폴더 정보 반복해서 가져옴
 *
 * @param argsPtr thread의 runtime 정보
 * @return 없음
 */
static void *dirListener(void *argsPtr);

/**
 * 현 폴더의 항목 정보 읽어들임
 *
 * @param dirToList 정보 읽어올 Directory
 * @param resultBuf 항목들의 stat 결과 저장할 공간
 * @param nameBuf 항목들의 이름 저장할 공간
 * @param bufLen 최대 읽어올 항목 수
 * @return 성공: (읽은 항목 수), 실패: -1
 */
static ssize_t listEntries(DIR *dirToList, struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen);

/**
 * 깨어날 시간 계산
 *
 * @param wakeupUs 대기 시간
 * @return 깨어날 시간 (Clock: CLOCK_REALTIME 기준)
 */
static struct timespec getWakeupTime(uint32_t wakeupUs);

/**
 * 디렉터리 변경
 * 
 * @param dir currentDir 변수 (해당 디렉터리에서 relative하게 탐색, 해당 변수에 새 directory 저장)
 * @param dirToMove 이동할 디렉터리명
 * @return 성공: 0, 실패: -1
 */
int changeDir(DIR **dir, char *dirToMove);


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
) {
    // Thread의 Runtime Variable 설정
    DirListenerArgs *newWinArg = argsArr + threadCnt;  // Thread 설정 포인터
    // 상태 값
    *statusFlags = 0;
    newWinArg->statusFlags = statusFlags;
    newWinArg->chdirIdx = chdirIdx;
    // 결과 Buffer
    newWinArg->statBuf = statBuf;
    newWinArg->nameBuf = entryNames;
    newWinArg->totalReadItems = totalReadItems;
    newWinArg->bufLen = bufLen;
    // Mutexes
    newWinArg->bufMutex = bufMutex;
    newWinArg->statusMutex = statusMutex;
    // Condition Variables
    newWinArg->condResumeThread = condResumeThread;

    // 디렉터리 열기
    // TODO: 시작 디렉터리 설정
    newWinArg->currentDir = opendir(".");
    if (newWinArg->currentDir == NULL)
        return -1;

    // 새 쓰레드 생성
    if (pthread_create(newThread, NULL, dirListener, newWinArg) == -1) {
        return -1;
    }
    return ++threadCnt;
}

void *dirListener(void *argsPtr) {
    DirListenerArgs args = *(DirListenerArgs *)argsPtr;
    struct timespec startTime, wakeupTime;
    bool changeDirRequested = false;
    uint64_t elapsedUSec;
    ssize_t readItems;
    int ret;

    pthread_mutex_lock(args.statusMutex);  // 상태 보호 Mutex 획득
    *args.statusFlags = LTHREAD_FLAG_RUNNING;
    pthread_mutex_unlock(args.statusMutex);  // 상태 보호 Mutex 해제

    while (1) {
        CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &startTime));  // iteration 시작 시간 저장

        // 정지 요청 확인
        pthread_mutex_lock(args.statusMutex);  // 상태 Flag 보호 Mutex 획득
        if (*args.statusFlags & LTHREAD_FLAG_STOP) {  // 종료 요청되었으면: 중지
            *args.statusFlags = 0;
            pthread_mutex_unlock(args.statusMutex);
            goto EXIT;
        }
        if (*args.statusFlags & LTHREAD_FLAG_CHANGE_DIR) {
            changeDirRequested = true;
            *args.statusFlags &= ~LTHREAD_FLAG_CHANGE_DIR;
        }
        pthread_mutex_unlock(args.statusMutex);  // 상태 Flag 보호 Mutex 해제

        // 현재 폴더 내용 가져옴
        pthread_mutex_lock(args.bufMutex);  // 결과값 보호 Mutex 획득

        // 폴더 변경 처리
        if (changeDirRequested)
            changeDir(&args.currentDir, args.nameBuf[*args.chdirIdx]);

        // 내용 가져오기
        readItems = listEntries(args.currentDir, args.statBuf, args.nameBuf, args.bufLen);
        if (readItems == -1)
            CHECK_FAIL(-1);
        *args.totalReadItems = readItems;

        pthread_mutex_unlock(args.bufMutex);  // 결과값 보호 Mutex 해제

        // 설정된 간격만큼 지연 및 종료 요청 처리
        pthread_mutex_lock(args.statusMutex);  // 상태 보호 Mutex 획득

        if (*args.statusFlags & LTHREAD_FLAG_STOP) {  // 종료 요청되었으면: 중지
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

ssize_t listEntries(DIR *dirToList, struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen) {
    // TODO: scandirat & versionsort 이용 구현
    // (해당 함수 사용 시, 별도 정렬 불필요)
    // (자동으로 malloc()됨 -> 현재 static buffer 관련 수정 필요, 구현 시 free() 유의하여 구현)
    // (주의: 해당 함수 glibc 비표준 함수임)

    size_t readItems = 0;
    errno = 0;  // errno 변수는 각 Thread별로 존재 -> Race Condition 없음

    rewinddir(dirToList);
    for (struct dirent *ent = readdir(dirToList); readItems < bufLen; ent = readdir(dirToList)) {  // 최대 buffer 길이 만큼의 항목들 읽어들임
        if (ent == NULL) {  // 읽기 끝
            if (errno != 0) {  // 오류 시
                return -1;
            }
            return readItems;  // 오류 없음 -> 계속 진행
        }
        strncpy(nameBuf[readItems], ent->d_name, MAX_NAME_LEN);  // 이름 복사
        nameBuf[readItems][MAX_NAME_LEN] = '\0';  // 끝에 null 문자 추가: 파일 이름 매우 긴 경우, strncpy()는 끝에 null문자 쓰지 않을 수도 있음
        if (stat(ent->d_name, resultBuf + readItems) == -1) {  // stat 읽어들임
            return -1;
        }
        errno = 0;
        readItems++;
    }
    return readItems;
}

int changeDir(DIR **dir, char *dirToMove) {
    // TODO: 디렉터리 변경 구현
    return -1;
}

struct timespec getWakeupTime(uint32_t wakeupUs) {
    struct timespec time;
    CHECK_FAIL(clock_gettime(CLOCK_REALTIME, &time));  // 현재 시간 가져옴
    uint64_t newNsec = time.tv_nsec + wakeupUs * 1000;  // 현재 시간 + 지연 시간 계산
    time.tv_sec += newNsec / (1000 * 1000 * 1000);  // 깨어날 초 설정
    time.tv_nsec = newNsec % (1000 * 1000 * 1000);  // 깨어날 나노초 설정
    return time;
}
