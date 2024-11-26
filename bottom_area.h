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
WINDOW* initBottomBox(int width, int startY);

/**
 * 하단 Window 삭제
 */
void delBottomBox(void);

/**
 * 진행률 정보 표시
 *
 * @param infos 파일 작업 진행률 정보
 * @return 실행 중인 작업 수
 */
int displayProgress(FileProgressInfo* infos);

/**
 * 매뉴얼 텍스트 표시
 * 대괄호([])로 묶인 부분은 역상으로 표시
 */
void displayManual(void);

/**
 * 하단 영역 업데이트
 * 진행 중인 작업이 있으면 진행률 표시, 없으면 매뉴얼 표시
 *
 * @param infos 진행률 정보
 */
void displayBottomBox(FileProgressInfo* infos);

#endif