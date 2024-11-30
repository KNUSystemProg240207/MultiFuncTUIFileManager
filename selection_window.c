#include <curses.h>
#include <panel.h>
#include <stdarg.h>
#include <string.h>

#include "colors.h"
#include "config.h"
#include "selection_window.h"


static WINDOW *selectionWindow;
static PANEL *selectionPanel;
static int selected;
static int selectionCnt;
static char titleBuf[MAX_SELECTION_TITLE_LEN + 1];
static int titleLen;
static char messageBuf[MAX_SELECTIONS][MAX_SELECTION_LEN + 1];
static int messageLen[MAX_SELECTIONS];


void initSelectionWindow(void) {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);

    selectionWindow = newwin(5, screenW - 10, (screenH - 5) / 2, 5);
    if (selectionWindow == NULL) {
        return;
    }
    // 팝업 배경색 설정
    if (isColorSafe) {
        wbkgd(selectionWindow, COLOR_PAIR(POPUP));
    }
    // 패널 생성
    selectionPanel = new_panel(selectionWindow);
    hide_panel(selectionPanel);
}

void showSelectionWindow(char *title, int selCnt, ...) {
    if (title != NULL) {
        strncpy(titleBuf, title, MAX_SELECTION_TITLE_LEN);
        titleBuf[MAX_SELECTION_TITLE_LEN] = '\0';
        titleLen = strlen(titleBuf);
    } else {
        titleBuf[0] = '\0';
        titleLen = 0;
    }

    va_list ap;
    va_start(ap, selCnt);
    selectionCnt = selCnt;
    for (int i = 0; i < selCnt && i < MAX_SELECTIONS; i++) {
        strncpy(messageBuf[i], va_arg(ap, char *), MAX_SELECTION_LEN);
        messageBuf[i][MAX_SELECTION_LEN] = '\0';
        messageLen[i] = strlen(messageBuf[i]);
    }
    va_end(ap);

    selected = 0;
    show_panel(selectionPanel);
}

void hideSelectionWindow(void) {
    hide_panel(selectionPanel);
}

void delSelectionWindow(void) {
    del_panel(selectionPanel);
    delwin(selectionWindow);
}

void setWindowSize(int windowWidth) {
    static int prevScreenH = 0, prevScreenW = 0;
    int screenH, screenW;
    // 현재 화면 크기 가져오기
    getmaxyx(stdscr, screenH, screenW);

    // 이전 창 크기와 비교
    if (prevScreenH == screenH && prevScreenW == screenW) {
        return;
    }
    prevScreenH = screenH;
    prevScreenW = screenW;

    // 윈도우, 패널 레이아웃 재배치
    wresize(selectionWindow, 5, windowWidth);
    replace_panel(selectionPanel, selectionWindow);
    move_panel(selectionPanel, (screenH - 5) / 2, (screenW - windowWidth) / 2);
}

void updateSelectionWindow(void) {
    // 선택 부분 길이 계산
    int selectionWidth = 2;
    for (int i = 0; i < selectionCnt; i++)
        selectionWidth += messageLen[i] + 2;

    // 창 폭 계산
    int windowWidth = ((titleLen > selectionWidth) ? titleLen : selectionWidth) + 4;

    // 창 폭 변경
    setWindowSize(windowWidth);

    werase(selectionWindow);
    applyColor(selectionWindow, POPUP);

    // 제목 부분 출력
    mvwaddstr(selectionWindow, 1, (windowWidth - titleLen) / 2, titleBuf);

    // 선택 부분 출력
    wmove(selectionWindow, 3, (windowWidth - selectionWidth) / 2);
    for (int i = 0; i < selectionCnt; i++) {
        if (i == selected)
            wattron(selectionWindow, A_REVERSE);
        wprintw(selectionWindow, "[%s]", messageBuf[i]);
        if (i == selected)
            wattroff(selectionWindow, A_REVERSE);
        waddch(selectionWindow, ' ');
    }

    mvwhline(selectionWindow, 2, 1, ACS_HLINE, windowWidth - 2);  // 구분 가로선 출력
    box(selectionWindow, 0, 0);  // 테두리 출력

    removeColor(selectionWindow, POPUP);
    top_panel(selectionPanel);
}

void selectionWindowSelNext(void) {
    if (selected == selectionCnt - 1)
        selected = 0;
    else
        selected++;
}

void selectionWindowSelPrevious(void) {
    if (selected == 0)
        selected = selectionCnt - 1;
    else
        selected--;
}

int selectionWindowGetSel(void) {
    return selected;
}
