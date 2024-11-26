#include <assert.h>
#include <curses.h>
#include <panel.h>
#include <stdlib.h>

#include "bottom_area.h"
#include "commons.h"
#include "config.h"
#include "file_operator.h"


/**
 * 진행률 정보 표시
 *
 * @param infos 파일 작업 진행률 정보
 * @return 실행 중인 작업 수
 */
static int displayProgress(FileProgressInfo *infos);

/**
 * 매뉴얼 텍스트 표시
 * 대괄호([])로 묶인 부분은 역상으로 표시
 */
static void displayManual(void);


static WINDOW *bottomBox;
static PANEL *bottomPanel;

// 매뉴얼 문자열을 bottom_area.c로 이동
static const char *const MANUAL1 = "[^ / v] Move         [< / >] Switch   [Enter] Open      [w] NameSort   [e] SizeSort   [r] DateSort";
static const char *const MANUAL2 = "[c / x] Copy / Cut   [v] Paste        [Delete] Delete   [p] Process    [q] Quit";


WINDOW *initBottomBox(int width, int startY) {
    assert((bottomBox = newwin(3, width, startY, 0)));
    assert((bottomPanel = new_panel(bottomBox)));
    return bottomBox;
}

void delBottomBox(void) {
    assert((del_panel(bottomPanel) != ERR));
    assert((delwin(bottomBox) != ERR));
}

void displayManual(void) {
    int width = getmaxx(bottomBox);

    // 첫 번째 줄
    wmove(bottomBox, 1, 1);
    for (int i = 0; MANUAL1[i] != '\0' && i < width - 2; i++) {
        if (MANUAL1[i] == '[') {
            wattron(bottomBox, A_REVERSE);
        }
        waddch(bottomBox, MANUAL1[i]);
        if (MANUAL1[i] == ']') {
            wattroff(bottomBox, A_REVERSE);
        }
    }

    // 두 번째 줄
    wmove(bottomBox, 2, 1);
    for (int i = 0; MANUAL2[i] != '\0' && i < width - 2; i++) {
        if (MANUAL2[i] == '[') {
            wattron(bottomBox, A_REVERSE);
        }
        waddch(bottomBox, MANUAL2[i]);
        if (MANUAL2[i] == ']') {
            wattroff(bottomBox, A_REVERSE);
        }
    }
}

int displayProgress(FileProgressInfo *infos) {
    int width = getmaxx(bottomBox);
    int x, y, w = width / 2 - 1;
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
                x = width / 2 + 1;
                y = 1;
                break;
            case 3:
                x = 1;
                y = 2;
                break;
            case 4:
                x = width / 2 + 1;
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

void updateBottomBox(FileProgressInfo *infos) {
    werase(bottomBox);
    mvwhline(bottomBox, 0, 0, ACS_HLINE, getmaxx(bottomBox));
    if (displayProgress(infos) == 0)
        displayManual();
}
