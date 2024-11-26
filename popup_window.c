#include <curses.h>
#include <panel.h>
#include <string.h>

#include "colors.h"
#include "config.h"
#include "popup_window.h"


static WINDOW *popupWindow;  // 주소 입력 받는 Window
static PANEL *popupWindowPanel;
static int charCount = 0;
static char inputBuf[PATH_MAX + 1];


void initPopupWindow() {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);
    int h = 3;  // 높이 1줄
    int w = screenW - 4;  // 좌우로 여백 2칸씩
    int y = screenH - 7;  // 세로 중앙
    int x = 2;  // 가로 여백 2칸

    popupWindow = newwin(h, w, y, x);
    if (popupWindow == NULL) {
        return;
    }
    // 팝업 배경색 설정
    if (isColorSafe) {
        wbkgd(popupWindow, COLOR_PAIR(POPUP));
    }
    // 패널 생성
    popupWindowPanel = new_panel(popupWindow);
    hide_panel(popupWindowPanel);
}

void showPopupWindow(char *title) {
    werase(popupWindow);  // 이전 내용 삭제
    box(popupWindow, 0, 0);  // 테두리 생성
    if (title != NULL) {
        wattron(popupWindow, A_REVERSE);
        mvwaddstr(popupWindow, 0, 1, title);
        wattroff(popupWindow, A_REVERSE);
    }
    show_panel(popupWindowPanel);
}

void hidePopupWindow() {
    hide_panel(popupWindowPanel);
    memset(inputBuf, '\0', sizeof(inputBuf));
    charCount = 0;
}

void delPopupWindow() {
    del_panel(popupWindowPanel);
    delwin(popupWindow);
}

void updatePopupWindow() {
    int width = getmaxx(popupWindow);
    int x = 1, y = 1;  // 여백 1칸
    int startIdx = 0;
    applyColor(popupWindow, POPUP);

    mvwhline(popupWindow, 1, 1, ' ', width - 2);  // 입력된 문자 삭제

    if (charCount > width - 2) {  // 문자열이 긴 경우
        startIdx = charCount - width + 2;  // 문자열의 마지막만 출력
        if (charCount < PATH_MAX) {  // 버퍼에 남은 공간 존재하면 - 커서 공백 출력할 공간 예약
            startIdx--;
        }
    }

    mvwaddstr(popupWindow, 1, 1, &inputBuf[startIdx]);

    // 커서 위치를 역상으로 표시 (버퍼가 비었으면 공백 표시)
    if (charCount < PATH_MAX) {
        wattron(popupWindow, A_REVERSE);
        waddch(popupWindow, ' ');
        wattroff(popupWindow, A_REVERSE);
    }

    removeColor(popupWindow, POPUP);
    top_panel(popupWindowPanel);
}

void putCharToPopup(char ch) {
    inputBuf[charCount++] = ch;
}

void popCharFromPopup() {
    inputBuf[--charCount] = '\0';
}

void getStringFromPopup(char *buffer) {
    strcpy(buffer, inputBuf);
}
