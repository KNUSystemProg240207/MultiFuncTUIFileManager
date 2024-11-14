#ifndef __FILE_OPS_H_INCLUDED__
#define __FILE_OPS_H_INCLUDED__

#include <sys/types.h>
#include "config.h"  // MAX_PATH_LEN, MAX_NAME_LEN 사용

#define MAX_NAME_LEN 256
#define COPY_BUFFER_SIZE 8192

typedef enum {  // _FileOperation 대신 간단히
    COPY,
    MOVE,
    DELETE
} FileOperation;

typedef struct {  // _FileTask 대신 간단히
    FileOperation type;  // FileOpration -> FileOperation으로 수정
    int srcDirFd;
    dev_t srcDevNo;
    char srcName[MAX_NAME_LEN];
    size_t fileSize;
    int dstDirFd;
    dev_t dstDevNo;
    char dstName[MAX_NAME_LEN];
} FileTask;

int executeFileOperation(FileOperation op);

/**
 * 파일 복사 함수
 * @param task 파일 작업 정보
 * @param offset 복사 시작 위치
 * @param size 복사할 크기
 * @return 성공 시 복사된 바이트 수, 실패 시 -1
 */
ssize_t copyFileOperation(FileTask *task, off_t offset, size_t size);

/**
 * 파일 이동 함수
 * @param task 파일 작업 정보
 * @return 성공 시 0, 실패 시 -1
 */
int moveFileOperation(FileTask *task);

/**
 * 파일 삭제 함수
 * @param task 파일 작업 정보
 * @return 성공 시 0, 실패 시 -1
 */
int deleteFileOperation(FileTask *task);

#endif