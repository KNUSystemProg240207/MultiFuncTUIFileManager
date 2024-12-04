#ifndef _FILE_FUNCTIONS_H_INCLUDED_
#define _FILE_FUNCTIONS_H_INCLUDED_

#include "file_operator.h"

/**
 * 파일/폴더 복사
 *
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
int copyFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress);

/**
 * 파일/폴더 이동
 *
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
int moveFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress);

/**
 * 파일/폴더 삭제
 *
 * @param src 삭제할 파일
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
int removeFile(SrcDstInfo *src, FileProgressInfo *progress);

/**
 * 폴더 생성
 *
 * @param src 생성할 폴더 정보
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
int makeDirectory(SrcDstInfo *src, FileProgressInfo *progress);

#endif
