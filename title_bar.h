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

/**
 * 화면 우상단에 현재 시간 표시 (또는 새로고침)
 */
void renderTime();

/**
 * 화면 상단 가운데에 경로 출력
 *
 * @param path 출력할 경로 문자열
 */
void printPath(char *path);

#endif
