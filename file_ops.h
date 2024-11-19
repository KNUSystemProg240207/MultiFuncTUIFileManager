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

/**
 * 파일 복사 작업 수행
 * 
 * @param src 원본 파일 정보
 * @param dst 대상 폴더 정보
 * @param progress 현재 진행 상황 ([7..0] 백분률 진행률 / [8] 복사중)
 * @param progressMutex 진행률 보호 뮤텍스
 * @return 성공: 0, 실패: -1
 */
int copyFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t progressMutex);

/**
 * 파일 이동 작업 수행
 * 
 * @param src 원본 파일 정보
 * @param dst 대상 폴더 정보
 * @param progress 현재 진행 상황 ([7..0] 백분률 진행률 / [9] 이동중)
 * @param progressMutex 진행률 보호 뮤텍스
 * @return 성공: 0, 실패: -1
 */
int moveFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t progressMutex);

/**
 * 파일 삭제 작업 수행
 * 
 * @param src 원본 파일 정보
 * @param progress 현재 진행 상황 ([7..0] 백분률 진행률 / [10] 삭제중)
 * @param progressMutex 진행률 보호 뮤텍스
 * @return 성공: 0, 실패: -1
 */
int removeFile(SrcDstInfo *src, uint16_t *progress, pthread_mutex_t progressMutex);

#endif