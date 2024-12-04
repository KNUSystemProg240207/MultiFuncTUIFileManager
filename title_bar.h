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
 * 제목 표시줄 자원 해제
 */
void delTitleBar(void);

/**
 * @brief 제목 표시줄의 내용을 갱신
 *
 * @param cwd 현재 작업 디렉토리 경로
 */
void updateTitleBar(const char *cwd, size_t cwdLen);


#endif
