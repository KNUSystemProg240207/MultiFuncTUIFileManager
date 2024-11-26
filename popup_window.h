#ifndef POPUP_WINDOW_H
#define POPUP_WINDOW_H

#include <curses.h>
#include <panel.h>

#include "config.h"

// 외부에서 접근할 함수 선언
void initpopupWin();  // popupWin 초기화
void updatePopupWin(char* title);  // popupWin 갱신 및 내용 반환
void hidePopupWindow();
void delPopupWindow();
void getString(char *buffer);
void addKey(char ch);  // 문자 추가
void deleteKey();  // 문자 삭제

#endif
