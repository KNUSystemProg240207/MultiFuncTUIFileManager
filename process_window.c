#include <limits.h>
#include <panel.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "colors.h"
#include "config.h"
#include "list_process.h"
#include "process_window.h"


static WINDOW *window;
static PANEL *panel;
static pthread_mutex_t *bufMutex;
static size_t *totalReadItems;
static Process *processes;


int initProcessWindow(
    pthread_mutex_t *_bufMutex,
    size_t *_totalReadItems,
    Process *_processes
) {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);
    int y = 2,
        x = 0,
        h = screenH - 5,
        w = screenW;

    window = newwin(h, w, y, x);
    if (window == NULL) {
        return -1;
    }

    panel = new_panel(window);
    if (window == NULL) {
        delwin(window);
        return -1;
    }
    hide_panel(panel);

    bufMutex = _bufMutex;
    totalReadItems = _totalReadItems;
    processes = _processes;

    // pthread_mutex_lock(&procWindow->visibleMutex);
    // procWindow->isWindowVisible = true;  // 프로세스 창 상태(열림)
    // pthread_mutex_unlock(&procWindow->visibleMutex);
    return 0;
}

void hideProcessWindow() {
    hide_panel(panel);
}

void delProcessWindow() {
    del_panel(panel);
    delwin(window);

    // pthread_mutex_lock(&procWindow->visibleMutex);
    // procWindow->isWindowVisible = false;  // 프로세스 창 상태(닫힘)
    // pthread_mutex_unlock(&procWindow->visibleMutex);
}

// void toggleProcWin(ProcessWindow *procWindow) {
//     pthread_mutex_lock(&procWindow->visibleMutex);
//     if (procWindow->isWindowVisible) {
//         pthread_mutex_unlock(&procWindow->visibleMutex);
//         closeProcWin(procWindow);
//     } else {
//         pthread_mutex_unlock(&procWindow->visibleMutex);
//         initProcessWindow(procWindow);
//     }
// }

int updateProcessWindow() {
    int winH, winW;

    getmaxyx(window, winH, winW);
    werase(window);  // <- 여기 한번 주목
    box(window, 0, 0);

    // 프로세스 창 업데이트
    pthread_mutex_lock(bufMutex);

    wbkgd(window, COLOR_PAIR(PRCSBGRND));

    getmaxyx(window, winH, winW);
    size_t itemsCnt = *totalReadItems;  // 읽어들인 총 항목 수

    int maxItemsToPrint = winH - 3;  // 상하단 여백 제외 창 높이에 비례한 출력 가능한 항목 수

    if (maxItemsToPrint > itemsCnt)
        maxItemsToPrint = itemsCnt;

    // wclear(window);  // <- 여기 한번 주목
    box(window, 0, 0);

    if (winW < 20) {
        mvwprintw(window, 1, 1, "%6s", "PID");
        for (int c = 0, i = *totalReadItems-1; c < maxItemsToPrint; c++, i--)
            mvwprintw(window, c + 2, 1, "%6d", processes[i].pid);
    } else if (winW < 30) {
        mvwprintw(window, 1, 1, "%6s %14s", "PID", "Rsize");
        for (int c = 0, i = *totalReadItems-1; c < maxItemsToPrint; c++, i--)
            mvwprintw(window, c + 2, 1, "%6d %14ld", processes[i].pid, processes[i].rsize);
    } else if (winW < 40) {
        mvwprintw(window, 1, 1, "%6s %14s %10s", "PID", "Rsize", "UTime");
        for (int c = 0, i = *totalReadItems-1; c < maxItemsToPrint; c++, i--)
            mvwprintw(window, c + 2, 1, "%6d %14ld %10lu", processes[i].pid, processes[i].rsize, processes[i].utime);
    } else if (winW < 50) {
        mvwprintw(window, 1, 1, "%6s %14s %10s %10s", "PID", "Rsize", "UTime", "STime");
        for (int c = 0, i = *totalReadItems-1; c < maxItemsToPrint; c++, i--)
            mvwprintw(window, c + 2, 1, "%6d %14ld %10lu %10lu", processes[i].pid, processes[i].rsize, processes[i].utime, processes[i].stime);
    } else if (winW < 80) {
        mvwprintw(window, 1, 1, "%6s %6s %14s %10s %10s", "PID", "State", "Rsize", "UTime", "STime");
        for (int c = 0, i = *totalReadItems-1; c < maxItemsToPrint; c++, i--)
            mvwprintw(window, c + 2, 1, "%6d %6c %14ld %10lu %10lu", processes[i].pid, processes[i].state, processes[i].rsize, processes[i].utime, processes[i].stime);
    } else {
        mvwprintw(window, 1, 1, "%6s %-*.*s %6s %14s %10s %10s", "PID", winW - 55, winW - 55, "Name", "State", "Rsize", "UTime", "STime");
        for (int c = 0, i = *totalReadItems-1; c < maxItemsToPrint; c++, i--)
            mvwprintw(window, c + 2, 1, "%6d %-*.*s %6c %14ld %10lu %10lu", processes[i].pid, winW - 55, winW - 55, processes[i].name, processes[i].state, processes[i].rsize, processes[i].utime, processes[i].stime);
    }
    whline(window, ' ', getmaxx(window) - getcurx(window) - 1);  // 남은 공간 공백 처리 (박스용 -1)

    pthread_mutex_unlock(bufMutex);

    top_panel(panel);
    return 0;
}
