#ifndef __DIR_LISTENER_H_INCLUDED__
#define __DIR_LISTENER_H_INCLUDED__

#include <dirent.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "config.h"
#include "dir_entry_utils.h"
#include "thread_commons.h"


#define DIRLISTENER_FLAG_CHANGE_DIR (1 << THREAD_FLAG_MSB)  // 디렉터리 변경 요청

#define DIRLISTENER_FLAG_SORT_NAME (0b00 << (THREAD_FLAG_MSB + 1))  // 정렬 기준: 이름
#define DIRLISTENER_FLAG_SORT_SIZE (0b01 << (THREAD_FLAG_MSB + 1))  // 정렬 기준: 크기
#define DIRLISTENER_FLAG_SORT_DATE (0b10 << (THREAD_FLAG_MSB + 1))  // 정렬 기준: 날짜
#define DIRLISTENER_FLAG_SORT_CRITERION_MASK (0b11 << (THREAD_FLAG_MSB + 1))  // 정렬 기준 마스크
#define DIRLISTENER_FLAG_SORT_REVERSE (1 << (THREAD_FLAG_MSB + 3))  // 내림차순 정렬


/**
 * @struct _DirEntry
 * 디렉토리 항목의 이름 및 파일 정보를 저장
 *
 * @var _DirEntry::entryName 파일/디렉토리 이름 (최대 MAX_NAME_LEN 길이)
 * @var _DirEntry::statEntry 파일/디렉토리의 stat 정보
 */
struct _DirEntry {
    char entryName[MAX_NAME_LEN + 1];  // 파일/디렉토리 이름
    struct stat statEntry;  // 파일/디렉토리의 stat 정보
};
typedef struct _DirEntry DirEntry;

/**
 * @struct _DirListenerArgs
 *
 * @var _DirListenerArgs::commonArgs Thread들 공통 공유 변수
 * @var _DirListenerArgs::chdirIdx 새 working directory의 index
 * @var _DirListenerArgs::statBuf 읽어들인 항목들의 stat 결과
 * @var _DirListenerArgs::nameBuf 읽어들인 항목들의 이름
 * @var _DirListenerArgs::totalReadItems 총 읽어들인 개수
 * @var _DirListenerArgs::bufMutex 결과값 보호 Mutex
 * @var _DirListenerArgs::dirMutex currentDir 보호 Mutex
 */
typedef struct _DirListenerArgs {
    ThreadArgs commonArgs;  // Thread들 공통 공유 변수
    // 상태 관련
    size_t chdirIdx;  // 새 working directory의 index
    DIR *currentDir;  // 현재 working directory (경고: 초기 Directory 설정 용도로만 접근, 이외 용도로 접근 금지!)
    // 결과 Buffer
    DirEntry dirEntries[MAX_DIR_ENTRIES];
    size_t totalReadItems;  // 총 읽어들인 개수
    // Mutexes
    pthread_mutex_t bufMutex;  // 결과값 보호 Mutex
    pthread_mutex_t dirMutex;  // currentDir 보호 Mutex
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
