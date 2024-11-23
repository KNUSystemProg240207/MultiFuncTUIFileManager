#ifndef __TITLE_BAR_H_INCLUDED__
#define __TITLE_BAR_H_INCLUDED__

#include <curses.h>


/**
 * 화면 상단 제목 Window 초기화
 *
 * @param width 화면 폭 (= 제목 Window 폭)
 * @return 상단 제목 Window
 */
WINDOW *initTitleBar(int width);

void delTitleBar(void);

/**
 * 화면 우상단에 현재 시간 표시 (또는 새로고침)
 */
void renderTime();

/**
 * 화면 상단 가운데에 경로 출력
 *
 * @param path 출력할 경로 문자열
 * @param pathLen (NULL-문자 제외한) 경로 문자열의 길이
 */
void printPath(char *path, size_t pathLen);

/**
 * @brief 제목 표시줄의 내용을 갱신합니다.
 *
 * @param cwd 현재 작업 디렉토리 경로
 */
void updateTitleBar(char *cwd, size_t cwdLen);

/**
 * @brief 제목 표시줄의 여백을 계산합니다.
 *
 * @param width 제목 표시줄의 총 너비
 */
void renderMargin(int width);

/**
 * @brief 제목 표시줄의 프로그램 이름을 출력합니다.
 */
void printProgramName(void);


#endif
