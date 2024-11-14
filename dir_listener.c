#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "commons.h"
#include "config.h"
#include "thread_commons.h"
#include "dir_listener.h"


static unsigned int threadCnt = 0;  // 생성된 Thread 개수

/**
 * (Thread의 loop 함수) 폴더 정보 반복해서 가져옴
 *
 * @param argsPtr thread의 runtime 정보
 * @return 성공: (읽은 항목 수), 실패: -1
 */
static int dirListener(void *argsPtr);

/**
 * 현 폴더의 항목 정보 읽어들임
 *
 * @param resultBuf 항목들의 stat 결과 저장할 공간
 * @param nameBuf 항목들의 이름 저장할 공간
 * @param bufLen 최대 읽어올 항목 수
 * @return 성공: (읽은 항목 수), 실패: -1
 */
static ssize_t listEntries(struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen);

/**
 * (Thread의 finish 함수) 종료 직전, 열려 있는 _currentDir 닫음
 *
 * @param argsPtr thread의 runtime 정보
 * @return 성공: 0, 실패: -1
 */
static int closeCurrentDir(void *argsPtr);



int startDirListender(
    pthread_t *newThread,
    DirListenerArgs *args
) {
    if (startThread(
        newThread, NULL, dirListener, closeCurrentDir,
        DIR_INTERVAL_USEC, &args->commonArgs, args
    ) == -1) {
        return -1;
    }
    return ++threadCnt;
}

int dirListener(void *argsPtr) {
    DirListenerArgs *args = (DirListenerArgs *)argsPtr;
    ssize_t readItems;
    // 현재 폴더 내용 가져옴
    pthread_mutex_lock(&args->bufMutex);  // 결과값 보호 Mutex 획득
    readItems = listEntries(args->statBuf, args->nameBuf, MAX_DIR_ENTRIES);  // 내용 가져오기
    if (readItems == -1)
        return -1;
    args->totalReadItems = readItems;
    pthread_mutex_unlock(&args->bufMutex);  // 결과값 보호 Mutex 해제
    return readItems;
}

ssize_t listEntries(struct stat *resultBuf, char (*nameBuf)[MAX_NAME_LEN + 1], size_t bufLen) {
    // TODO: scandirat & versionsort 이용 구현
    // (해당 함수 사용 시, 별도 정렬 불필요)
    // (자동으로 malloc()됨 -> 현재 static buffer 관련 수정 필요, 구현 시 free() 유의하여 구현)
    // (주의: 해당 함수 glibc 비표준 함수임)

    DIR *dir = opendir(".");
    if (dir == NULL) {
        return -1;
    }

    size_t readItems = 0;
    errno = 0;  // errno 변수는 각 Thread별로 존재 -> Race Condition 없음
    for (struct dirent *ent = readdir(dir); readItems < bufLen; ent = readdir(dir)) {  // 최대 buffer 길이 만큼의 항목들 읽어들임
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

int closeCurrentDir(void *argsPtr) {
    return closedir(((DirListenerArgs *)argsPtr)->_currentDir) == 0;
}
