#ifndef __DIR_ENTRY_UTILS_H_INCLUDED__
#define __DIR_ENTRY_UTILS_H_INCLUDED__
#include "dir_window.h"

/**
 * 파일 이름을 지정된 최대 표시 길이에 맞게 줄임
 *
 * @param fileName 수정할 파일 이름 (null-terminated 문자열)
 * @return 축약한 파일명 (static buffer에 저장됨 -> 다시 호출 시 값 덮어씌워짐)
 *
 * @details
 * - 파일 이름이 `MAX_DISPLAY_LEN`을 초과할 경우: 이름이 생략 기호(`...`) 삽입되어 축약
 * - 파일 확장자는 유지되며, 확장자를 제외한 부분이 생략
 * - 확장자가 너무 길 경우, 확장자를 포함한 문자열을 `MAX_DISPLAY_LEN` 이내로 잘라냄
 */
char *truncateFileName(const char *fileName);

/**
 * 파일 이름이 숨김 파일인지 확인
 *
 * @param fileName 확인할 파일 이름 (null-terminated 문자열)
 * @return 숨김 파일이면 1, 그렇지 않으면 0
 *
 * @details
 * - 숨김 파일: '.'으로 시작하는 파일
 * - 현재 디렉토리(`.`) 또는 상위 디렉토리(`..`)는 제외
 */
int isHidden(const char *fileName);

/**
 * 파일이 이미지 파일인지 확인
 *
 * @param fileName 확인할 파일 이름 (null-terminated 문자열)
 * @return 이미지 파일이면 1, 그렇지 않으면 0
 *
 * @details
 * - 지원되는 이미지 확장자는 다음과 같습니다: `.jpg`, `.jpeg`, `.png`, `.gif`, `.bmp`
 * - 확장자는 대소문자를 구분하지 않습니다.
 */
int isImageFile(const char *fileName);

/**
 * 파일이 실행 파일인지 확인
 *
 * @param fileName 확인할 파일 이름 (null-terminated 문자열)
 * @return 실행 파일이면 1, 그렇지 않으면 0
 *
 * @details
 * - 지원되는 실행 파일 확장자는 다음과 같습니다: `.exe`, `.out`
 * - 확장자는 대소문자를 구분하지 않습니다.
 */
int isEXE(const char *fileName);

/**
 * 디렉토리 항목 배열 정렬
 *
 * @param dirEntries 정렬할 디렉토리 항목 배열 (DirEntry 구조체 배열).
 * @param flags 정렬 기준과 방향을 나타내는 비트 플래그:
 *               - 기준: `SORT_NAME`, `SORT_SIZE`, `SORT_DATE`
 *               - 방향: `SORT_ASCENDING`, `SORT_DESCENDING`
 * @param totalReadItems 정렬할 항목의 개수.
 *
 * @details
 * - 기준 플래그와 방향 플래그를 조합하여 정렬 수행.
 * - 기준이 동일하면 이름 기준으로 정렬.
 * - 항목이 없거나 배열이 NULL이면 동작하지 않음.
 */
void applySorting(DirEntry *dirEntries, uint16_t flags, size_t totalReadItems);


#endif
