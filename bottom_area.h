#ifndef __FOOTER_AREA_H_INCLUDED__
#define __FOOTER_AREA_H_INCLUDED__

#include <curses.h>

#include "config.h"
#include "file_operator.h"

/**
 * 화면 하단 단축키 표시 Window 초기화
 *
 * 
 * @param width 화면 폭 (= 단축키 Window 폭)
 * @param startY 화면 기준 단축키 Window 최상단의 y좌표
 * @return 하단 단축키 Window
 */
WINDOW *initBottomBox(int width, int startY);

/**
 * 화면 하단 진행률 표시 Window 업데이트
 *
 * @param infos 진행률 정보
 */
int displayProgress(FileProgressInfo *infos);
void displayManual(char* manual);
void displayBottomBox(FileProgressInfo *infos, char* manual);

#endif
