#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "commons.h"
#include "title_bar.h"


static WINDOW *titleBar;
static unsigned int w;
static unsigned int centerAreaStart;
static unsigned int centerAreaEnd;


WINDOW *initTitleBar(int width) {
    titleBar = newwin(1, width, 0, 0);
    CHECK_NULL(titleBar);
    CHECK_CURSES(mvwaddstr(titleBar, 0, 0, PROG_NAME));

    w = width;

    int progLen = strlen(PROG_NAME);
    int lenMax = progLen > DATETIME_LEN ? progLen : DATETIME_LEN;
    centerAreaStart = lenMax;
    centerAreaEnd = width - lenMax;

    return titleBar;
}

void renderTime() {
    static char buf[DATETIME_LEN + 1];

    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) == -1) {
        perror("Time");
        exit(-1);
    }
    struct tm *dt = localtime(&now.tv_sec);

    DT_TO_STR(buf, dt);
    mvwaddstr(titleBar, 0, w - DATETIME_LEN, buf);
}

void printPath(char *path) {
    int pathLen = strlen(path);
    int titleBarSize = centerAreaEnd - centerAreaStart;
    int restSpace = titleBarSize - pathLen;
    mvwaddnstr(titleBar, 0, centerAreaStart + (restSpace > 0 ? restSpace / 2 : 0), path, titleBarSize);
}
