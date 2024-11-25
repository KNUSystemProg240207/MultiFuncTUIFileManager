#include <assert.h>
#include <curses.h>
#include <panel.h>
#include <stdlib.h>

#include "bottom_area.h"
#include "commons.h"
#include "config.h"
#include "file_operator.h"


static WINDOW *bottomBox;
static PANEL *bottomPanel;

WINDOW *initBottomBox(int width, int startY) {
    assert((bottomBox = newwin(2, width, startY, 0)));
    assert((bottomPanel = new_panel(bottomBox)));
    return bottomBox;
}

void delBottomBox(void) {
    assert((del_panel(bottomPanel) != ERR));
    assert((delwin(bottomBox) != ERR));
}

int displayProgress(FileProgressInfo *infos) {
    int width = getmaxx(bottomBox);
    int x, y, w = width / 2 - 1;
    char operation;

    werase(bottomBox);
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
                y = 0;
                break;
            case 2:
                x = width / 2 + 1;
                y = 0;
                break;
            case 3:
                x = 1;
                y = 1;
                break;
            case 4:
                x = width / 2 + 1;
                y = 1;
                break;
        }
        mvwprintw(
            bottomBox, y, x, "%c %*.*s %.3d%%",
            operation, w - 7, w - 7, infos[i].name,
            (infos[i].flags & PROGRESS_PERCENT_MASK) >> PROGRESS_PERCENT_START
        );
        pthread_mutex_unlock(&infos[i].flagMutex);
    }

    //    wrefresh(bottomBox);

    return runningWins;
}

void displayManual(char *manual1, char *manual2) {
    int width = getmaxx(bottomBox);

    int x = 1, y = 0;
    for (int i = 0; manual1[i] != '\0'; i++) {
        if (x >= width - 1) {  // 화면 너비 초과 시
            break;
        }
        if (manual1[i] != '\n') {  // 줄바꿈 문자가 아니면 출력
            mvwaddch(bottomBox, y, x, manual1[i]);  // 한 글자씩 출력
            x++;  // 다음 위치로 이동
        }
    }

    x = 1, y = 1;
    for (int i = 0; manual2[i] != '\0'; i++) {
        if (x >= width - 1) {  // 화면 너비 초과 시
            break;
        }
        if (manual2[i] != '\n') {  // 줄바꿈 문자가 아니면 출력
            mvwaddch(bottomBox, y, x, manual2[i]);  // 한 글자씩 출력
            x++;  // 다음 위치로 이동
        }
    }

    // wrefresh(bottomBox);  // 화면 업데이트
}

void displayBottomBox(FileProgressInfo *infos, char *manual1, char *manual2) {
    if (displayProgress(infos) == 0)
        displayManual(manual1, manual2);
}
