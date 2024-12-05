#ifndef _FOOTER_AREA_H_INCLUDED_
#define _FOOTER_AREA_H_INCLUDED_

#include <curses.h>

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
 * 하단 영역 업데이트
 * 진행 중인 작업이 있으면 진행률 표시, 없으면 매뉴얼 표시
 *
 * @param infos 진행률 정보
 */
void updateBottomBox(FileProgressInfo* infos);

/**
 * 하단 영역에 출력될 메시지 설정
 * 다음 updateBottomBox때부터 실제로 출력됨
 *
 * @param msg 출력할 (null-terminated) 문자열
 * @param framesToShow 보여줄 시간, Frame (=updateBottomBox 호출 횟수) 단위
 */
void displayBottomMsg(const char* msg, int framesToShow);

/**
 * 하단 영역에 출력된 메시지가 있으면, 지움
 * 다음 updateBottomBox때 실제로 지워짐
 */
void clearBottomMsg();

#endif