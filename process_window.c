#include <limits.h>
#include <panel.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"
#include "list_process.h"
#include "proc_win.h"


int initProcWin(ProcWin *procWindow) {
    int y, x, h, w;

    // 위치 계산
    if (calculateProcWinPos(&y, &x, &h, &w) == -1) {
        return -1;
    }

    // 창 생성
    procWindow->win = newwin(h, w, y, x);
    if (procWindow->win == NULL) {
        return -1;
    }

    // 패널 생성
    procWindow->panel = new_panel(procWindow->win);

    // 변수 설정
    procWindow->totalReadItems = 0;

    pthread_mutex_lock(&procWindow->visibleMutex);
    procWindow->isWindowVisible = true;  // 프로세스 창 상태(열림)
    pthread_mutex_unlock(&procWindow->visibleMutex);
    return 0;
}

void closeProcWin(ProcWin *procWindow) {
    // 패널 삭제
    del_panel(procWindow->panel);

    delwin(procWindow->win);  // 창 삭제
    pthread_mutex_lock(&procWindow->visibleMutex);
    procWindow->isWindowVisible = false;  // 프로세스 창 상태(닫힘)
    pthread_mutex_unlock(&procWindow->visibleMutex);
}

void toggleProcWin(ProcWin *procWindow) {
    pthread_mutex_lock(&procWindow->visibleMutex);
    if (procWindow->isWindowVisible) {
        pthread_mutex_unlock(&procWindow->visibleMutex);
        closeProcWin(procWindow);
    } else {
        pthread_mutex_unlock(&procWindow->visibleMutex);
        initProcWin(procWindow);
    }
}

int calculateProcWinPos(int *y, int *x, int *h, int *w) {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);

    *y = 2;
    *x = 0;
    *h = screenH - 5;
    *w = screenW;

    return 0;
}

int updateProcWin(ProcWin *procWindow) {
    int winH, winW;
    ssize_t itemsCnt = 0;
    int maxItemsToPrint;


    // 프로세스 창 업데이트
    pthread_mutex_lock(&procWindow->statMutex);

    getmaxyx(procWindow->win, winH, winW);
    itemsCnt = procWindow->totalReadItems;  // 읽어들인 총 항목 수

    maxItemsToPrint = winH - 3;  // 상하단 여백 제외 창 높이에 비례한 출력 가능한 항목 수

    if (maxItemsToPrint > itemsCnt)
        maxItemsToPrint = itemsCnt;

    // wclear(procWindow->win);  // <- 여기 한번 주목
    box(procWindow->win, 0, 0);

    if (winW < 20)
        mvwprintw(procWindow->win, 1, 1, "%-6s", "PID");
    else if (winW < 30)
        mvwprintw(procWindow->win, 1, 1, "%-6s %-14s", "PID", "VSize");
    else if (winW < 40)
        mvwprintw(procWindow->win, 1, 1, "%-6s %-14s %-10s", "PID", "VSize", "UTime");
    else if (winW < 50)
        mvwprintw(procWindow->win, 1, 1, "%-6s %-14s %-10s %-10s", "PID", "VSize", "UTime", "STime");
    else if (winW < 80)
        mvwprintw(procWindow->win, 1, 1, "%-6s %-6s %-14s %-10s %-10s", "PID", "State", "VSize", "UTime", "STime");
    else
        mvwprintw(procWindow->win, 1, 1, "%-6s %-35s %-6s %-14s %-10s %-10s", "PID", "Name", "State", "VSize", "UTime", "STime");
    // 가장 큰 메모리를 차지하는 항목부터 출력
    for (int i = 0; i < maxItemsToPrint; i++) {
        // pointerArray를 사용하여 메모리 크기 내림차순으로 접근
        int idx = procWindow->totalReadItems - 1 - i;  // 정렬된 배열의 끝에서부터 접근
        ProcInfo *proc = procWindow->procEntries[idx];

        // 프로세스 정보 출력
        if (winW < 20)
            mvwprintw(procWindow->win, i + 2, 1, "%-6d", proc->pid);
        else if (winW < 30)
            mvwprintw(procWindow->win, i + 2, 1, "%-6d %-14lu", proc->pid, proc->vsize);
        else if (winW < 40)
            mvwprintw(procWindow->win, i + 2, 1, "%-6d %-14lu %-10lu", proc->pid, proc->vsize, proc->utime);
        else if (winW < 50)
            mvwprintw(procWindow->win, i + 2, 1, "%-6d %-14lu %-10lu %-10lu", proc->pid, proc->vsize, proc->utime, proc->stime);
        else if (winW < 80)
            mvwprintw(procWindow->win, i + 2, 1, "%-6d %-6c %-14lu %-10lu %-10lu", proc->pid, proc->state, proc->vsize, proc->utime, proc->stime);
        else
            mvwprintw(procWindow->win, i + 2, 1, "%-6d %-35s %-6c %-14lu %-10lu %-10lu", proc->pid, proc->name, proc->state, proc->vsize, proc->utime, proc->stime);
    }
    // 창 새로고침
    // wrefresh(procWindow->win);  // <- 주목

    top_panel(procWindow->panel);

    // 패널 업데이트
    update_panels();
    doupdate();


    pthread_mutex_unlock(&procWindow->statMutex);
    return 0;
}
