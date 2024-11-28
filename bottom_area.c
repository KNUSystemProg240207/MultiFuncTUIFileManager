#include <assert.h>
#include <curses.h>
#include <panel.h>
#include <stdlib.h>
#include <string.h>

#include "bottom_area.h"
#include "commons.h"
#include "config.h"
#include "file_operator.h"

/**
 * '하단 영역' 크기 조절 필요하면 조절
 *
 * @param newAreaWidth '하단 영역'의 너비 (항상 돌려줌)
 * @return 크기 조정 여부
 */
static bool checkAndResizeArea(int *newAreaWidth);

/**
 * 구분용 수평선을 '하단 영역' 최상단에 그린 후, message 있으면 표시
 *
 * @param winWidth '하단 영역'의 크기
 */
static void printHLineAndMsg(int winWidth);

/**
 * 진행률 정보 표시
 *
 * @param infos 파일 작업 진행률 정보
 * @param winWidth '하단 영역'의 크기
 * @return 실행 중인 작업 수
 */
static int displayProgress(FileProgressInfo *infos, int winWidth);

/**
 * 매뉴얼 텍스트 표시
 * 대괄호([])로 묶인 부분은 역상으로 표시
 *
 * @param winWidth '하단 영역'의 크기
 */
static void displayManual(int screenW);


static WINDOW *bottomBox;
static PANEL *bottomPanel;
static char msgBuf[MAX_BOTTOMBOX_MSG_LEN + 1];
static int msgLen;
static int msgLeftFrames;


WINDOW *initBottomBox(int width, int startY) {
    assert((bottomBox = newwin(3, width, startY, 0)));
    assert((bottomPanel = new_panel(bottomBox)));
    return bottomBox;
}

void delBottomBox(void) {
    assert((del_panel(bottomPanel) != ERR));
    assert((delwin(bottomBox) != ERR));
}

void updateBottomBox(FileProgressInfo *infos) {
    int winWidth;
    werase(bottomBox);
    checkAndResizeArea(&winWidth);
    if (displayProgress(infos, winWidth) == 0)
        displayManual(winWidth);
    printHLineAndMsg(winWidth);
}

bool checkAndResizeArea(int *newAreaWidth) {
    static int prevH = 0, prevW = 0;
    int h, w;

    getmaxyx(stdscr, h, w);
    *newAreaWidth = w;

    // 화면 크기 변경 감지
    if (prevH == h && prevW == w)
        return false;

    prevH = h;
    prevW = w;

    // 레이아웃 재배치
    wresize(bottomBox, 3, w);  // 높이 3, 너비 stdscr 사이즈 윈도우 생성
    replace_panel(bottomPanel, bottomBox);  // 교체
    move_panel(bottomPanel, h - 3, 0);  // 옮기기
    top_panel(bottomPanel);

    return true;
}

void displayBottomMsg(char *msg, int framesToShow) {
    strncpy(msgBuf, msg, MAX_BOTTOMBOX_MSG_LEN);
    msgBuf[MAX_BOTTOMBOX_MSG_LEN] = '\0';
    msgLen = strlen(msgBuf);
    msgLeftFrames = framesToShow;
}

void clearBottomMsg() {
    msgLeftFrames = 0;
}

void printHLineAndMsg(int winWidth) {
    mvwhline(bottomBox, 0, 0, ACS_HLINE, winWidth);
    if (msgLeftFrames > 0) {
        msgLeftFrames--;
        int printWidth = msgLen;
        if (printWidth > winWidth - 6)
            printWidth = winWidth - 6;
        wattron(bottomBox, A_REVERSE);
        mvwaddnstr(bottomBox, 0, (winWidth - printWidth) / 2, msgBuf, printWidth);
        wattroff(bottomBox, A_REVERSE);
    }
}

void displayManual(int screenW) {
    /*
    버그 발생
        터미널 높이를 아예 안보일 정도로 줄였다가 다시 늘리면, 정상 출력이 되지만
        그냥 상태에서 터미널 높이를 LINES 12줄 정도 이하로 줄이면, 절반이 출력이 안 됨
    */

    const char *manual1, *manual2;

    // 창 크기에 따라 출력 내용 결정
    if (screenW >= 121) {  // 전체 출력 7열
        manual1 = "[^ / v] Move     [^c / ^v] Copy / Cut   [F2] Rename         [Delete] Delete   [p] Process   [w] NameSort   [e] SizeSort";
        manual2 = "[< / >] Switch   [  ^v   ] Paste        [F4] Move to Path   [Enter ] Open     [q] Quit      [r] DateSort";
    } else if (screenW >= 106) {  // 6열
        manual1 = "[^ / v] Move     [^c / ^v] Copy / Cut   [F2] Rename         [Delete] Delete   [p] Process   [w] NameSort";
        manual2 = "[< / >] Switch   [  ^v   ] Paste        [F4] Move to Path   [Enter ] Open     [q] Quit      [r] DateSort";
    } else if (screenW >= 91) {  // 5열
        manual1 = "[^ / v] Move     [^c / ^v] Copy / Cut   [F2] Rename         [Delete] Delete   [p] Process";
        manual2 = "[< / >] Switch   [  ^v   ] Paste        [F4] Move to Path   [Enter ] Open     [q] Quit";
    } else if (screenW >= 77) {  // 4열
        manual1 = "[^ / v] Move     [^c / ^v] Copy / Cut   [F2] Rename         [Delete] Delete";
        manual2 = "[< / >] Switch   [  ^v   ] Paste        [F4] Move to Path   [Enter ] Open";
    } else if (screenW >= 59) {  // 3열
        manual1 = "[^ / v] Move     [^c / ^v] Copy / Cut   [F2] Rename";
        manual2 = "[< / >] Switch   [  ^v   ] Paste        [F4] Move to Path";
    } else {  // 최소 출력 2열
        manual1 = "[^ / v] Move     [^c / ^v] Copy / Cut";
        manual2 = "[< / >] Switch   [  ^v   ] Paste";
    }

    // 첫 번째 줄 출력
    wmove(bottomBox, 1, 1);
    for (int i = 0; manual1[i] != '\0' && i < screenW - 2; i++) {
        if (manual1[i] == '[') {
            wattron(bottomBox, A_REVERSE);
        }
        waddch(bottomBox, manual1[i]);
        if (manual1[i] == ']') {
            wattroff(bottomBox, A_REVERSE);
        }
    }

    // 두 번째 줄 출력
    wmove(bottomBox, 2, 1);
    for (int i = 0; manual2[i] != '\0' && i < screenW - 2; i++) {
        if (manual2[i] == '[') {
            wattron(bottomBox, A_REVERSE);
        }
        waddch(bottomBox, manual2[i]);
        if (manual2[i] == ']') {
            wattroff(bottomBox, A_REVERSE);
        }
    }
}

int displayProgress(FileProgressInfo *infos, int winWidth) {
    int x, y, w = winWidth / 2 - 1;
    char operation;

    int runningWins = 0;
    for (int i = 0; i < MAX_FILE_OPERATORS; i++) {
        pthread_mutex_lock(&infos[i].flagMutex);
        switch (infos[i].flags & PROGRESS_BITS) {
            case PROGRESS_COPY:
                operation = 'C';
                runningWins++;
                break;
            case PROGRESS_MOVE:
                operation = 'M';
                runningWins++;
                break;
            case PROGRESS_DELETE:
                operation = 'D';
                runningWins++;
                break;
            default:
                pthread_mutex_unlock(&infos[i].flagMutex);
                continue;
        }
        switch (runningWins) {
            case 1:
                x = 1;
                y = 1;
                break;
            case 2:
                x = winWidth / 2 + 1;
                y = 1;
                break;
            case 3:
                x = 1;
                y = 2;
                break;
            case 4:
                x = winWidth / 2 + 1;
                y = 2;
                break;
        }
        mvwprintw(
            bottomBox, y, x, "%c %*.*s %.3d%%",
            operation, w - 7, w - 7, infos[i].name,
            (infos[i].flags & PROGRESS_PERCENT_MASK) >> PROGRESS_PERCENT_START
        );
        pthread_mutex_unlock(&infos[i].flagMutex);
    }

    return runningWins;
}
