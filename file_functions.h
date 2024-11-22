#ifndef __FILE_FUNCTIONS_H_INCLUDED__
#define __FILE_FUNCTIONS_H_INCLUDED__

#include <pthread.h>
#include <stdint.h>

#include "file_operator.h"

/**
 * 파일 복사
 *
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
int copyFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress);

/**
 * 파일 이동
 *
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
int moveFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress);

/**
 * 파일 삭제
 *
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
int removeFile(SrcDstInfo *src, FileProgressInfo *progress);

#endif
