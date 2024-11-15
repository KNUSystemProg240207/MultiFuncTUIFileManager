#ifndef __FILE_OPERATOR_H_INCLUDED__
#define __FILE_OPERATOR_H_INCLUDED__

#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>

#include "config.h"
#include "thread_commons.h"


typedef enum _FileOpration {
    COPY,
    MOVE,
    DELETE
} FileOpration;

typedef struct _FileTask {
    FileOpration type;
    int srcDirFd;
    dev_t srcDevNo;
    char srcName[MAX_NAME_LEN];
    size_t fileSize;
    int dstDirFd;
    dev_t dstDevNo;
    char dstName[MAX_NAME_LEN];
} FileTask;


/**
 * @struct _FileOperatorArgs
 *
 * @var _FileOperatorArgs::commonArgs Thread들 공통 공유 변수
 * @var _FileOperatorArgs::opearatingFile 작업 중인 파일명
 * @var _FileOperatorArgs::progress 진행률(백분률) & 진행 중인 작업 종류
 * @var _FileOperatorArgs::pipeEnd 명령 읽어올, pipe의 read용 끝단
 */
typedef struct _FileOperatorArgs {
    ThreadArgs commonArgs;  // Thread들 공통 공유 변수
    // 상태 관련
    char opearatingFile[MAX_NAME_LEN];
    uint16_t progress;
    // 명령 읽어올 곳
    int pipeEnd;
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
