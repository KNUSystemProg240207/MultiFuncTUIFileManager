#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "commons.h"
#include "dir_window.h"
#include "footer_area.h"
#include "list_dir.h"
#include "title_bar.h"


WINDOW *titleBar, *bottomBox;

static pthread_t threadListDir[MAX_DIRWINS];
static pthread_mutex_t statMutex[MAX_DIRWINS];
static struct stat statEntries[MAX_DIRWINS][MAX_STAT_ENTRIES];
static char entryNames[MAX_DIRWINS][MAX_NAME_LEN + 1];
static pthread_cond_t condStopTrd[MAX_DIRWINS];
static pthread_mutex_t stopTrdMutex[MAX_DIRWINS];
static size_t totalReadItems[MAX_DIRWINS] = { 0 };
// static unsigned int dirWinCnt;

static void initVariables(void);
static void initScreen(void);
static void initThreads(void);
static void mainLoop(void);
static void cleanup(void);


int main(int argc, char **argv) {
    atexit(cleanup);

    initVariables();
    initScreen();
    // initThreads();
    mainLoop();

    CHECK_CURSES(delwin(titleBar));
    CHECK_CURSES(delwin(bottomBox));

    return 0;
}

void initVariables(void) {
    for (int i = 0; i < MAX_DIRWINS; i++) {
        pthread_mutex_init(statMutex + i, NULL);
        pthread_cond_init(condStopTrd + i, NULL);
        pthread_mutex_init(stopTrdMutex + i, NULL);
    }
}

void initScreen(void) {
    if (initscr() == NULL) {
        fprintf(stderr, "Window initialization FAILED\n");
        exit(1);
    }
    CHECK_CURSES(cbreak());
    CHECK_CURSES(noecho());
    CHECK_CURSES(curs_set(0));

    int h, w;
    getmaxyx(stdscr, h, w);

    CHECK_FALSE1(has_colors(), "Doesn't support color\n", 1);
    CHECK_CURSES(start_color());
    if (can_change_color() == TRUE) {
        CHECK_CURSES(init_color(COLOR_WHITE, 1000, 1000, 1000));
    }

    titleBar = initTitleBar(w);
    bottomBox = initBottomBox(w, h - 2);
    CHECK_CURSES(mvhline(1, 0, ACS_HLINE, w));
    CHECK_CURSES(mvhline(h - 3, 0, ACS_HLINE, w));

    // initDirWin(&statMutex[0], statEntries[0], entryNames[0], &totalReadItems[0]);
}

void initThreads(void) {
    startDirListender(
        &threadListDir[0], &statMutex[0],
        statEntries[0], entryNames[0], MAX_STAT_ENTRIES,
        &totalReadItems[0], &condStopTrd[0], &stopTrdMutex[0]
    );
}

void mainLoop(void) {
    CHECK_CURSES(nodelay(stdscr, TRUE));
    struct timespec startTime;
    uint64_t elapsedUSec;
    while (1) {
        // Start time
        clock_gettime(CLOCK_MONOTONIC, &startTime);

        // Process key input
        for (int ch = getch(); ch != ERR; ch = getch()) {
            switch (ch) {
                case 'q':
                case 'Q':
                    goto CLEANUP;
            }
        }

        // Do render
        renderTime();
        // updateWins();
        // TODO: get current window's cwd
        char *cwd = getcwd(NULL, 0);
        printPath(cwd);
        free(cwd);

        // Refresh screen
        CHECK_CURSES(wrefresh(titleBar));
        CHECK_CURSES(wrefresh(bottomBox));
        refresh();

        // Limit frame rate by insert delay: delay - elapsed time
        elapsedUSec = getElapsedTime(startTime);
        if (FRAME_INTERVAL_USEC > elapsedUSec) {
            usleep(FRAME_INTERVAL_USEC - elapsedUSec);
        }
    }
CLEANUP:
    return;
}

void cleanup(void) {
    endwin();
}
