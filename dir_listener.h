#ifndef __DIR_LISTENER_H_INCLUDED__
#define __DIR_LISTENER_H_INCLUDED__

#include <dirent.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "config.h"
#include "thread_commons.h"


/**
 * @struct _DirListenerArgs
 *
 * @var _DirListenerArgs::commonArgs Thread들 공통 공유 변수
 * @var _DirListenerArgs::chdirIdx 새 working directory의 index
 * @var _DirListenerArgs::statBuf 읽어들인 항목들의 stat 결과
 * @var _DirListenerArgs::nameBuf 읽어들인 항목들의 이름
 * @var _DirListenerArgs::totalReadItems 총 읽어들인 개수
 * @var _DirListenerArgs::bufMutex 결과값 buffer 보호 Mutex
 * 
 * @note `_`으로 시작하는 변수: 내부적으로 사용하는 변수, 외부에서 사용 금지!
 */
typedef struct _DirListenerArgs {
    ThreadArgs commonArgs;  // Thread들 공통 공유 변수
    // 상태 관련
    size_t chdirIdx;  // 새 working directory의 index
    // 결과 Buffer
    struct stat statBuf[MAX_DIR_ENTRIES];  // 읽어들인 항목들의 stat 결과
    char nameBuf[MAX_DIR_ENTRIES][MAX_NAME_LEN + 1];  // 읽어들인 항목들의 이름
    size_t totalReadItems;  // 총 읽어들인 개수
    // Mutexes
    pthread_mutex_t bufMutex;  // 결과값 buffer 보호 Mutex

    // 비공유 변수
    DIR *_currentDir;  // 현재 working directory
} DirListenerArgs;

/**
 * 새 Directory Listener Thread 시작
 *
 * @param newThread (반환) 새 쓰레드
 * @param args 공유 변수 저장하는 구조체의 Pointer
 * @return 성공: 0, 실패: -1
 */
int startDirListender(
    pthread_t *newThread,
    DirListenerArgs *args
);

#endif
