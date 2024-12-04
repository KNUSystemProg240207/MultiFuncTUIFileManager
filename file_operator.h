#ifndef __FILE_OPERATOR_H_INCLUDED__
#define __FILE_OPERATOR_H_INCLUDED__

#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>

#include "config.h"
#include "thread_commons.h"


#define PROGRESS_PREV_CP (1 << 10)
#define PROGRESS_PREV_MV (2 << 10)
#define PROGRESS_PREV_RM (3 << 10)
#define PROGRESS_PREV_MKDIR (4 << 10)
#define PROGRESS_PREV_MASK (7 << 10)
#define PROGRESS_PREV_FAIL (1 << 13)

#define PROGRESS_OP_CP (1 << 7)
#define PROGRESS_OP_MV (2 << 7)
#define PROGRESS_OP_RM (3 << 7)
#define PROGRESS_OP_MKDIR (4 << 7)
#define PROGRESS_OP_MASK (7 << 7)
#define PROGRESS_PERCENT_START 0
#define PROGRESS_PERCENT_MASK (0x7f << PROGRESS_PERCENT_START)


typedef enum _FileOperation {
    COPY,
    MOVE,
    DELETE,
    MKDIR
} FileOperation;

typedef struct _SrcDstInfo {
    dev_t devNo;
    mode_t mode;
    int dirFd;
    char name[NAME_MAX + 1];
    size_t fileSize;
} SrcDstInfo;

typedef struct _FileTask {
    FileOperation type;
    SrcDstInfo src;
    SrcDstInfo dst;
} FileTask;

typedef struct _FileProgressInfo {
    char name[NAME_MAX];  // 작업중인 파일 이름
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
    pthread_mutex_t *pipeReadMutex;  // 명령 읽기 보호 Mutex
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
