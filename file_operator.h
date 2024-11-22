#ifndef __FILE_OPERATOR_H_INCLUDED__
#define __FILE_OPERATOR_H_INCLUDED__

#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>

#include "config.h"
#include "thread_commons.h"


#define PROGRESS_COPY (1 << 10)
#define PROGRESS_MOVE (2 << 10)
#define PROGRESS_DELETE (3 << 10)
#define PROGRESS_BITS (3 << 10)
#define PROGRESS_PERCENT_START 1
#define PROGRESS_PERCENT_MASK (0x7f << PROGRESS_PERCENT_START)


typedef enum _FileOperation {
    COPY,
    MOVE,
    DELETE
} FileOperation;

typedef struct _SrcDstFile {
    dev_t devNo;
    int dirFd;
    char name[MAX_NAME_LEN];
    size_t fileSize;
} SrcDstInfo;

typedef struct _FileTask {
    FileOperation type;
    SrcDstInfo src;
    SrcDstInfo dst;
} FileTask;

typedef struct _FileProgressInfo {
    char name[MAX_NAME_LEN];  // 작업중인 파일 이름
    uint16_t flags;  // 진행률 Bit Field
    pthread_mutex_t flagMutex;  // 진행률 보호 Mutex
} FileProgressInfo;

/**
 * @struct _FileOperatorArgs
 *
 * @var _FileOperatorArgs::commonArgs Thread들 공통 공유 변수
 * @var _FileOperatorArgs::progressInfo 진행 상태 공유 변수
 * @var _FileOperatorArgs::pipeEnd 명령 읽어올, pipe의 read용 끝단
 * @var _FileOperatorArgs::pipeReadMutex 명령 읽기 보호 Mutex
 */
typedef struct _FileOperatorArgs {
    ThreadArgs commonArgs;  // Thread들 공통 공유 변수
    FileProgressInfo *progressInfo;  // 진행 상태 공유 변수
    // 명령 관련
    int pipeEnd;  // 명령 읽어올, pipe의 read용 끝
    pthread_mutex_t pipeReadMutex;  // 명령 읽기 보호 Mutex
} FileOperatorArgs;


/**
 * 새 File Operator Thread 시작
 *
 * @param newThread (반환) 새 쓰레드
 * @param args 공유 변수 저장하는 구조체의 Pointer
 * @return 성공: 0, 실패: -1
 */
int startFileOperator(
    pthread_t *newThread,
    FileOperatorArgs *args
);

#endif
