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

void updateProcessWindow() {
    int winH, winW;

    getmaxyx(window, winH, winW);
    werase(window);
    box(window, 0, 0);

    // 프로세스 창 업데이트
    pthread_mutex_lock(bufMutex);

    size_t itemsCnt = *totalReadItems;  // 읽어들인 총 항목 수

    int maxItemsToPrint = winH - 3;  // 상하단 여백 제외 창 높이에 비례한 출력 가능한 항목 수

    if (maxItemsToPrint > itemsCnt)
        maxItemsToPrint = itemsCnt;

    // 배경 색칠
    if (isColorSafe) wbkgd(window, COLOR_PAIR(PRCSBGRND));

    // 테이블 헤더 출력
    printTableHeader(window, winW);

    // 색상 적용
    applyColor(window, PRCSFILE);

    // 프로세스 정보 출력
    printProcessInfo(window, winW, processes, maxItemsToPrint, *totalReadItems);

    // 색상 해제
    removeColor(window, PRCSFILE);

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
    for (int c = 0, i = totalItems; c < maxItems; c++, i--) {
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

// 프로세스가 점유하고 있는 메모리 사이즈를 포맷팅합니다.
const char *formatSize(unsigned long size) {  // const char *로 수정을 막음
    static char formatted_size[16];
    const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit_index = 0;

    while (size >= (1 << 10) && unit_index < 4) {
        size >>= 10;  // 성능을 위해 비트 시프트로 연산, 2^10을 나누는 나눔
        unit_index++;
    }
    /* 사이즈 형식으로 포매팅해서 스트링 리턴 */
    snprintf(formatted_size, sizeof(formatted_size), "%lu%s", size, units[unit_index]);
    return formatted_size;
}

// tick을 초 단위로 변경합니다.
unsigned long ticksToSeconds(unsigned long ticks) {
    return ticks / sysconf(_SC_CLK_TCK);  // 시스템 틱을 초로 변환.
}