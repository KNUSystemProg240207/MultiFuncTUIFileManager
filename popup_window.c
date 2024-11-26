#include <curses.h>
#include <panel.h>
#include <string.h>

#include "config.h"

static WINDOW *popupWin;  // 주소 입력 받는 Window
static PANEL *popupWinPanel;
static int charCount = 0;
static char fileAddress[MAX_PATH_LEN + 1];

void initpopupWin() {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);
    int h = 3;  // 높이 1줄
    int w = screenW - 4;  // 좌우로 여백 2칸씩
    int y = (screenH - h) / 2;  // 세로 중앙
    int x = 2;  // 가로 여백 2칸

    popupWin = newwin(h, w, y, x);
    if (popupWin == NULL) {
        return;
    }

    // 패널 생성
    popupWinPanel = new_panel(popupWin);
    hide_panel(popupWinPanel);
}
void updatePopupWin(char *title) {
    werase(popupWin);  // 이전 내용 삭제
    box(popupWin, 0, 0);  // 테두리 생성

    int width = getmaxx(popupWin);
    int x = 1, y = 1;  // 여백 1칸
    int startIdx = 0;

    if (charCount > width) {
        startIdx = charCount - width;  // 마지막 문자 중심으로 잘라냄
    }

    if(title != NULL){
        wattron(popupWin, A_REVERSE);
        mvwaddstr(popupWin, y-1, x, title); // 제목 역상으로 출력
        wattroff(popupWin, A_REVERSE);
    }

    for (int i = startIdx; fileAddress[i] != '\0'; i++) {
        if (x >= width - 1) {
            break;  // 창 너비 초과 시 멈춤
        }
        mvwaddch(popupWin, y, x, fileAddress[i]);
        x++;
    }

    // 커서 위치를 역상으로 표시 (버퍼가 비었으면 공백 표시)
    if (charCount < MAX_PATH_LEN) {
        wattron(popupWin, A_REVERSE);
        mvwaddch(popupWin, y, x, ' ');
        wattroff(popupWin, A_REVERSE);
    }

    top_panel(popupWinPanel);
}

void hidePopupWindow() {
    hide_panel(popupWinPanel);
    fileAddress[0] = '\0';
    charCount = 0;
}

void delPopupWindow() {
    del_panel(popupWinPanel);
    delwin(popupWin);

    // pthread_mutex_lock(&procWindow->visibleMutex);
    // procWindow->isWindowVisible = false;  // 프로세스 창 상태(닫힘)
    // pthread_mutex_unlock(&procWindow->visibleMutex);
}

void getString(char* buffer){
    strcpy(buffer, fileAddress);
}


void addKey(char ch) {
    if (charCount < MAX_PATH_LEN && charCount >= 0) {
        fileAddress[charCount++] = ch;
        fileAddress[charCount] = '\0';
    }
}

void deleteKey() {
    if (charCount <= MAX_PATH_LEN && charCount > 0) {
        fileAddress[--charCount] = '\0';
    }
}