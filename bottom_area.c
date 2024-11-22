#include <curses.h>
#include <stdlib.h>

#include "bottom_area.h"
#include "commons.h"
#include "config.h"
#include "file_operator.h"


static WINDOW *bottomBox;


WINDOW *initBottomBox(int width, int startY) {
    CHECK_NULL(bottomBox = newwin(2, width, startY, 0));
    return bottomBox;
}

void displayProgress(FileProgressInfo *infos) {
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

    wrefresh(bottomBox);
}
