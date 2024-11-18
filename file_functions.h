#ifndef __FILE_FUNCTIONS_H_INCLUDED__
#define __FILE_FUNCTIONS_H_INCLUDED__

#include <pthread.h>
#include <stdint.h>

#include "file_operator.h"

/**
 * 파일 복사 수행
 *
 * @param src 원본 파일 정보
 * @param dst 대상 폴더 정보
 * @param progress 현재 진행 상황 ([7..0] 백분률 진행률 / [8] 복사중)
 * @param progressMutex 대상 폴더 정보
 */
int copyFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t *progressMutex);

/**
 * 파일 이동 수행
 *
 * @param src 원본 파일 정보
 * @param dst 대상 폴더 정보
 * @param progress 현재 진행 상황 ([7..0] 백분률 진행률 / [9] 이동중)
 * @param progressMutex 대상 폴더 정보
 */
int moveFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t *progressMutex);

/**
 * 파일 삭제 수행
 *
 * @param src 원본 파일 정보
 * @param progress 현재 진행 상황 ([7..0] 백분률 진행률 / [10] 삭제중)
 * @param progressMutex 대상 폴더 정보
 */
int removeFile(SrcDstInfo *src, uint16_t *progress, pthread_mutex_t *progressMutex);

#endif
