#ifndef POPUP_WINDOW_H
#define POPUP_WINDOW_H

#include <curses.h>
#include <panel.h>

#include "config.h"

// 외부에서 접근할 함수 선언
void initPopupWindow(void);
void showPopupWindow(const char *title);
void hidePopupWindow(void);
void delPopupWindow(void);

void updatePopupWindow(void);

void putCharToPopup(char ch);
void popCharFromPopup(void);
void getStringFromPopup(char *buffer);

#endif
