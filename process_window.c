#include <limits.h>
#include <panel.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "colors.h"
#include "commons.h"
#include "config.h"
#include "list_process.h"
#include "process_window.h"


static WINDOW *window;
static PANEL *panel;
static pthread_mutex_t *bufMutex;
static size_t *totalReadItems;
static Process *processes;


/**
 * 프로세스 창의 테이블 헤더를 출력
 *
 * @param win 출력할 창(WINDOW 포인터)
 * @param winW 창의 너비
 */
static void printTableHeader(WINDOW *win, int winW);

/**
 * 프로세스 정보를 출력
 *
 * @param win 출력할 창(WINDOW 포인터)
 * @param winW 창의 너비
 * @param processes 출력할 프로세스 배열
 * @param maxItems 출력할 최대 항목 수
 * @param totalItems 총 항목 수
 */
static void printProcessInfo(WINDOW *win, int winW, Process *processes, int maxItems, int totalItems);

/**
 * @brief 클럭 틱(tick)을 초(second)로 변환
 * @param ticks 클럭 틱 값
 * @return 변환된 초 단위 시간
 */
static unsigned long ticksToSeconds(unsigned long ticks);

/**
 * 프로세스 창 크기 업데이트
 *
 * @param winH 업데이트 받을 창의 높이
 * @param winW 업데이트 받을 창의 너비
 */
static void setProcessWinSize(int *winH, int *winW);


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

    return 0;
}

void hideProcessWindow() {
    hide_panel(panel);
}

void delProcessWindow() {
    del_panel(panel);
    delwin(window);
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

void setProcessWinSize(int *winH, int *winW) {
    static int prevScreenH = 0, prevScreenW = 0;
    int screenH, screenW;
    // 현재 화면 크기 가져오기
    getmaxyx(stdscr, screenH, screenW);
    int y = 2,
        x = 0,
        h = screenH - 5,
        w = screenW;

    // 이전 창 크기와 비교
    if (prevScreenH != screenH || prevScreenW != screenW) {
        // 최신화
        prevScreenH = screenH;
        prevScreenW = screenW;

        // 윈도우, 패널 레이아웃 재배치
        wresize(window, h, w);
        replace_panel(panel, window);
        move_panel(panel, y, x);

        *winW = screenW;
        *winH = screenH;
    } else {
        getmaxyx(window, *winH, *winW);  // 이전 창 크기와 같으면, 그대로 전달
    }
}

void updateProcessWindow() {
    int winH, winW;

    // getmaxyx(window, winH, winW);
    setProcessWinSize(&winH, &winW);
    werase(window);
    box(window, 0, 0);

    // 프로세스 창 업데이트
    pthread_mutex_lock(bufMutex);

    size_t itemsCnt = *totalReadItems;  // 읽어들인 총 항목 수
    int maxItemsToPrint = winH - 3;  // 상하단 여백 제외 창 높이에 비례한 출력 가능한 항목 수
    if (maxItemsToPrint > itemsCnt)
        maxItemsToPrint = itemsCnt;

    // 배경 색칠
    if (isColorSafe)
        wbkgd(window, COLOR_PAIR(PRCSBGRND));
    printTableHeader(window, winW);  // 테이블 헤더 출력
    applyColor(window, PRCSFILE);  // 색상 적용
    printProcessInfo(window, winW, processes, maxItemsToPrint, *totalReadItems);  // 프로세스 정보 출력
    removeColor(window, PRCSFILE);  // 색상 해제
    whline(window, ' ', getmaxx(window) - getcurx(window) - 1);  // 남은 공간 공백 처리 (박스용 -1)

    pthread_mutex_unlock(bufMutex);
    top_panel(panel);
}

// 테이블 헤더 출력
void printTableHeader(WINDOW *win, int winW) {
    if (winW < 20) {
        mvwprintw(win, 1, 1, "%6s", "PID");
    } else if (winW < 30) {
        mvwprintw(win, 1, 1, "%6s %14s", "PID", "Rsize");
    } else if (winW < 40) {
        mvwprintw(win, 1, 1, "%6s %14s %10s", "PID", "Rsize", "UTime");
    } else if (winW < 50) {
        mvwprintw(win, 1, 1, "%6s %14s %10s %10s", "PID", "Rsize", "UTime", "STime");
    } else if (winW < 80) {
        mvwprintw(win, 1, 1, "%6s %6s %14s %10s %10s", "PID", "State", "Rsize", "UTime", "STime");
    } else {
        mvwprintw(win, 1, 1, "%6s %-*.*s %6s %14s %10s %10s", "PID", winW - 55, winW - 55, "Name", "State", "Rsize", "UTime", "STime");
    }
}

// 프로세스 정보 출력
void printProcessInfo(WINDOW *win, int winW, Process *processes, int maxItems, int totalItems) {
    for (int c = 0, i = *totalReadItems - 1; c < maxItems; c++, i--) {
        if (winW < 20) {
            mvwprintw(win, c + 2, 1, "%6d", processes[i].pid);
        } else if (winW < 30) {
            mvwprintw(win, c + 2, 1, "%6d %14s", processes[i].pid, formatSize(processes[i].rsize));
        } else if (winW < 40) {
            mvwprintw(win, c + 2, 1, "%6d %14s %9lus", processes[i].pid, formatSize(processes[i].rsize), ticksToSeconds(processes[i].utime));
        } else if (winW < 50) {
            mvwprintw(win, c + 2, 1, "%6d %14s %9lus %9lus", processes[i].pid, formatSize(processes[i].rsize), ticksToSeconds(processes[i].utime), ticksToSeconds(processes[i].stime));
        } else if (winW < 80) {
            mvwprintw(win, c + 2, 1, "%6d %6c %14s %9lus %9lus", processes[i].pid, processes[i].state, formatSize(processes[i].rsize), ticksToSeconds(processes[i].utime), ticksToSeconds(processes[i].stime));
        } else {
            mvwprintw(win, c + 2, 1, "%6d %-*.*s %6c %14s %9lus %9lus", processes[i].pid, winW - 55, winW - 55, processes[i].name, processes[i].state, formatSize(processes[i].rsize), ticksToSeconds(processes[i].utime), ticksToSeconds(processes[i].stime));
        }
    }
}

// tick을 초 단위로 변경합니다.
unsigned long ticksToSeconds(unsigned long ticks) {
    return ticks / sysconf(_SC_CLK_TCK);  // 시스템 틱을 초로 변환.
}
