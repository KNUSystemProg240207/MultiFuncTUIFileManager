#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "commons.h"
#include "config.h"
#include "dir_entry_utils.h"
#include "dir_listener.h"
#include "thread_commons.h"

static unsigned int threadCnt = 0;  // 생성된 Thread 개수
extern int directoryOpenArgs;  // main.c 참조

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
 * @param dirToList 정보 읽어올 Directory
 * @param resultBuf 항목들의 stat 결과 저장할 공간
 * @param nameBuf 항목들의 이름 저장할 공간
 * @param bufLen 최대 읽어올 항목 수
 * @return 성공: (읽은 항목 수), 실패: -1
 */
static ssize_t listEntries(DIR *dirToList, DirEntry *dirEntry, size_t bufLen);

/**
 * 디렉터리 변경
 *
 * @param dir currentDir 변수 (해당 디렉터리에서 relative하게 탐색, 해당 변수에 새 directory 저장)
 * @param dirToMove 이동할 디렉터리명
 * @return 성공: 0, 실패: -1
 */
int changeDir(DIR **dir, char *dirToMove);

/**
 * (Thread의 finish 함수) 종료 직전, 열려 있는 currentDir 닫음
 *
 * @param argsPtr thread의 runtime 정보
 * @return 성공: 0, 실패: -1
 */
static int closeCurrentDir(void *argsPtr);


int startDirListender(
    pthread_t *newThread,
    DirListenerArgs *args
) {
    if (threadCnt >= MAX_DIRWINS)
        return -1;
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
    bool changeDirRequested = false;

    // 폴더 변경 요청 확인
    pthread_mutex_lock(&args->commonArgs.statusMutex);  // 상태 Flag 보호 Mutex 획득
    if (args->commonArgs.statusFlags & DIRLISTENER_FLAG_CHANGE_DIR) {
        changeDirRequested = true;
        args->commonArgs.statusFlags &= ~DIRLISTENER_FLAG_CHANGE_DIR;
    }
    pthread_mutex_unlock(&args->commonArgs.statusMutex);  // 상태 Flag 보호 Mutex 해제

    // 폴더 변경 처리
    pthread_mutex_lock(&args->dirMutex);  // 현재 Directory 보호 Mutex 획득
    if (changeDirRequested)
        changeDir(&args->currentDir, args->newCwdPath);

    // 현재 폴더 내용 가져옴
    pthread_mutex_lock(&args->bufMutex);  // 결과값 보호 Mutex 획득
    readItems = listEntries(args->currentDir, args->dirEntries, MAX_DIR_ENTRIES);  // 내용 가져오기
    pthread_mutex_unlock(&args->dirMutex);  // 현재 Directory 보호 Mutex 해제
    if (readItems == -1) {
        pthread_mutex_unlock(&args->bufMutex);  // 결과값 보호 Mutex 해제
        return -1;
    }
    if (readItems == 0)
        readItems = 0;
    args->totalReadItems = readItems;

    applySorting(args->dirEntries, args->commonArgs.statusFlags, readItems);  // 불러온 목록 정렬

    pthread_mutex_unlock(&args->bufMutex);  // 결과값 보호 Mutex 해제
    return readItems;
}

ssize_t listEntries(DIR *dirToList, DirEntry *dirEntries, size_t bufLen) {
    size_t readItems = 0;
    int fdDir = dirfd(dirToList);
    errno = 0;  // errno 변수는 각 Thread별로 존재 -> Race Condition 없음

    rewinddir(dirToList);
    for (struct dirent *ent = readdir(dirToList); readItems < bufLen; ent = readdir(dirToList)) {  // 최대 buffer 길이 만큼의 항목들 읽어들임
        if (ent == NULL) {  // 읽기 끝
            if (errno != 0) {  // 오류 시
                return -1;
            }
            return readItems;  // 오류 없음 -> 계속 진행
        }
        if (strcmp(ent->d_name, ".") == 0) {  // 현재 디렉토리 "."는 받아오지 않음(정렬을 위함)
            continue;
        }
        strncpy(dirEntries[readItems].entryName, ent->d_name, NAME_MAX);  // 이름 복사
        dirEntries[readItems].entryName[NAME_MAX] = '\0';  // 끝에 null 문자 추가: 파일 이름 매우 긴 경우, strncpy()는 끝에 null문자 쓰지 않을 수도 있음
        if (fstatat(fdDir, ent->d_name, &(dirEntries[readItems].statEntry), AT_SYMLINK_NOFOLLOW) == -1) {  // stat 읽어들임
            return -1;
        }
        errno = 0;
        readItems++;
    }
    if (readItems == 0)
        readItems = 0;
    return readItems;
}

int changeDir(DIR **dir, char *dirToMove) {
    DIR *currentDir = *dir;

    // 전달받은 currentDirent의, 내부적으로 사용되는 file descriptor 받아옴
    // (주의: 이 파일 descriptor 자체를 close()하면 안 됨: closedir()할 때 같이 닫힘)
    int fdDir = dirfd(currentDir);
    if (fdDir == -1)  // 실패 시 -> -1 리턴, 종료
        return -1;

    // 현재 directory 기준, relative path 가리키는 폴더 열기: '전달된 (directory) file descriptor에 대한 상대 경로'의 파일을 open()하는 openat() 사용
    int fdParent = openat(fdDir, dirToMove, directoryOpenArgs);
    if (fdParent == -1)  // 실패 시 -> -1 리턴, 종료
        return -1;

    // 전달된 currentDirent close하기: dirfd로 받은 fdDir도 같이 닫힘
    if (closedir(currentDir) == -1)  // 실패 시 -> -1 리턴, 종료
        return -1;

    // fdParent가 가리키는 file descriptor open
    // (주의: 전달한 fdParent 또한 별도로 close()하면 안 됨: DIR 내부에서 사용)
    DIR *result;
    result = fdopendir(fdParent);
    if (!result)  // 실패 시 -> -1 리턴, 종료
        return -1;
    *dir = result;

    return 0;
}

int closeCurrentDir(void *argsPtr) {
    return closedir(((DirListenerArgs *)argsPtr)->currentDir) == 0;
}
