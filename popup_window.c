#include <curses.h>
#include <panel.h>
#include <string.h>

#include "colors.h"
#include "config.h"
#include "popup_window.h"


static WINDOW *popupWindow;  // 주소 입력 받는 Window
static PANEL *popupWindowPanel;
static int charCount = 0;
static char popupTitleBuf[MAX_POPUP_TITLE_LEN + 1];
static char inputBuf[PATH_MAX + 1];


void initPopupWindow(void) {
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
    if (title != NULL) {
        strncpy(popupTitleBuf, title, MAX_POPUP_TITLE_LEN);
        popupTitleBuf[MAX_POPUP_TITLE_LEN] = '\0';
    } else {
        popupTitleBuf[0] = '\0';
    }
    show_panel(popupWindowPanel);
}

void hidePopupWindow(void) {
    hide_panel(popupWindowPanel);
    memset(inputBuf, '\0', sizeof(inputBuf));
    charCount = 0;
}

void delPopupWindow(void) {
    del_panel(popupWindowPanel);
    delwin(popupWindow);
}

void setPopUpSize(int *width) {
    static int prevScreenH = 0, prevScreenW = 0;
    int screenH, screenW;
    // 현재 화면 크기 가져오기
    getmaxyx(stdscr, screenH, screenW);

    *width = screenW;  // 창 크기 전달

    // 이전 창 크기와 비교
    if (prevScreenH == screenH && prevScreenW == screenW) {
        return;
    }

    // 최신화
    prevScreenH = screenH;
    prevScreenW = screenW;

    // 윈도우, 패널 레이아웃 재배치
    wresize(popupWindow, 3, screenW - 4);
    replace_panel(popupWindowPanel, popupWindow);
    move_panel(popupWindowPanel, screenH - 7, 2);
}

void updatePopupWindow(void) {
    int screenWidth;
    setPopUpSize(&screenWidth);

    box(popupWindow, 0, 0);  // 테두리 생성
    wattron(popupWindow, A_REVERSE);
    mvwaddstr(popupWindow, 0, 1, popupTitleBuf);
    wattroff(popupWindow, A_REVERSE);

    int startIdx = 0;
    applyColor(popupWindow, POPUP);

    mvwhline(popupWindow, 1, 1, ' ', screenWidth - 6);  // 입력된 문자 삭제

    if (charCount > screenWidth - 2) {  // 문자열이 긴 경우
        startIdx = charCount - screenWidth + 2;  // 문자열의 마지막만 출력
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

void popCharFromPopup(void) {
    inputBuf[--charCount] = '\0';
}

void getStringFromPopup(char *buffer) {
    strcpy(buffer, inputBuf);
}
