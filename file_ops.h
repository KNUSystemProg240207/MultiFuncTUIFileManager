#ifndef __FILE_OPS_H_INCLUDED__
#define __FILE_OPS_H_INCLUDED__

#include <sys/types.h>
#include "config.h"

typedef enum _FileOperation {
    COPY,
    MOVE,
    DELETE
} FileOperation;

typedef struct _FileTask {
    FileOperation type;
    int srcDirFd;
    dev_t srcDevNo;
    char srcName[MAX_NAME_LEN];
    size_t fileSize;
    int dstDirFd;
    dev_t dstDevNo;
    char dstName[MAX_NAME_LEN];
} FileTask;

/**
 * 파일 복사 작업 수행
 * 
 * @param task 파일 작업 정보
 * @return 성공: 0, 실패: -1
 */
int copyFile(FileTask *task);

/**
 * 파일 이동 작업 수행
 * 
 * @param task 파일 작업 정보
 * @return 성공: 0, 실패: -1
 */
int moveFile(FileTask *task);

/**
 * 파일 삭제 작업 수행
 * 
 * @param task 파일 작업 정보
 * @return 성공: 0, 실패: -1
 */
int deleteFile(FileTask *task);

#endif