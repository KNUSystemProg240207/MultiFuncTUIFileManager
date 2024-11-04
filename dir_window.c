#include <curses.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>

#include "config.h"
#include "dir_window.h"


struct _DirWin {
    WINDOW *win;
    unsigned int order;
    size_t currentPos;
    pthread_mutex_t *statMutex;
    struct stat *statEntries;
    char (*entryNames)[MAX_NAME_LEN + 1];
    size_t *totalReadItems;
};
typedef struct _DirWin DirWin;


static unsigned int winCnt;
static DirWin windows[MAX_DIRWINS];


static int calculateWinPos(unsigned int winNo, int *y, int *x, int *h, int *w);


int initDirWin(
    pthread_mutex_t *statMutex,
    struct stat *statEntries,
    char (*entryNames)[MAX_NAME_LEN + 1],
    size_t *totalReadItems
) {
    int y, x, h, w;
    winCnt++;
    if (calculateWinPos(winCnt - 1, &y, &x, &h, &w) == -1) {
        return -1;
    }
    WINDOW *newWin = newwin(h, w, y, x);
    if (newWin == NULL) {
        winCnt--;
        return -1;
    }
    windows[winCnt - 1] = (DirWin) {
        .win = newWin,
        .order = winCnt - 1,
        .currentPos = 0,
        .statMutex = statMutex,
        .statEntries = statEntries,
        .entryNames = entryNames,
        .totalReadItems = totalReadItems,
    };
    mvwaddstr(newWin, 0, 0, "Test | Hello, World!");
    wrefresh(newWin);
    return winCnt;
}

int updateWins(void) {
    int ret, winH, winW, curY, curX, linesTopToCenter;
    size_t itemsCnt;
    ssize_t startIdx, endIdx;
    DirWin *win;
    for (int i = 0; i < winCnt; i++) {
        win = windows + i;
        ret = pthread_mutex_trylock(win->statMutex);
        if (ret != 0)
            continue;
        itemsCnt = *win->totalReadItems;

        getmaxyx(win->win, winH, winW);
        linesTopToCenter = (winW - 1) / 2;
        startIdx = win->currentPos - linesTopToCenter;
        endIdx = startIdx + winW - 1;

        if (startIdx < 0) {
            startIdx = 0;
        } else if (itemsCnt >= endIdx) {
            startIdx -= itemsCnt - endIdx + 1;
            endIdx = itemsCnt - 1;
        }

        int l;
        for (l = 0; l < winH && l < itemsCnt; l++) {
            mvwaddstr(win->win, l, 0, win->entryNames[startIdx + l]);
            getyx(win->win, curY, curX);
            whline(win->win, ' ', winW - curX);
            char *tmp = win->entryNames[startIdx + l];
            int foo = 1 + 1;
        }
        for (; l < winH; l++) {
            mvwhline(win->win, l, 0, ' ', winW);
        }
        wrefresh(win->win);

        pthread_mutex_unlock(win->statMutex);
    }
    return 0;
}

int calculateWinPos(unsigned int winNo, int *y, int *x, int *h, int *w) {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);
    switch (winCnt) {
        case 1:
            *y = 2;
            *x = 0;
            *h = screenH - 5;
            *w = screenW;
            return 0;
        // Todo: Implement below cases
        // case 2:
        //     return 0;
        // case 3:
        //     return 0;
        default:
            return -1;
    }
}

