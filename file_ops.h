#ifndef __FILE_OPS_H_INCLUDED__
#define __FILE_OPS_H_INCLUDED__

#include <sys/types.h>
#include <pthread.h>
#include "config.h"

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
    uint16_t *flags;
    char *srcName;
    char *dstName;
} FileProgressInfo;

#define PROGRESS_COPY (1 << 10)
#define PROGRESS_MOVE (2 << 10)
#define PROGRESS_DELETE (3 << 10)
#define PROGRESS_PERCENT_START 1
#define PROGRESS_PERCENT_MASK (0x7f << PROGRESS_PERCENT_START)

/**
 * 파일 복사
 * 
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @param progressMutex 진행률 보호 Mutex
 * @return 성공: 0, 실패: -1
 */
int copyFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress, pthread_mutex_t *progressMutex);

/**
 * 파일 이동
 * 
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @param progressMutex 진행률 보호 Mutex
 * @return 성공: 0, 실패: -1
 */
int moveFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress, pthread_mutex_t *progressMutex);

/**
 * 파일 삭제
 * 
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @param progressMutex 진행률 보호 Mutex
 * @return 성공: 0, 실패: -1
 */
int removeFile(SrcDstInfo *src, FileProgressInfo *progress, pthread_mutex_t *progressMutex);

#endif