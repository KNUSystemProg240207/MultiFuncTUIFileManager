#ifndef __FILE_OPS_H_INCLUDED__
#define __FILE_OPS_H_INCLUDED__

/**
 * 선택된 파일/폴더 복사
 * 
 * @param src 원본 경로
 * @param dst 대상 경로
 * @return 성공: 0, 실패: -1
 */
int copyFile(const char *src, const char *dst);

/**
 * 선택된 파일/폴더 이동
 * 
 * @param src 원본 경로
 * @param dst 대상 경로
 * @return 성공: 0, 실패: -1
 */
int moveFile(const char *src, const char *dst);

/**
 * 선택된 파일/폴더 삭제
 * 
 * @param path 삭제할 경로
 * @return 성공: 0, 실패: -1
 */
int removeFile(const char *path);

#endif