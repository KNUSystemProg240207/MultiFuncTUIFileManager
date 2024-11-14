#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
#include "config.h"
#include "proc_win.h"

static ProcWin procWindow;  // 프로세스 관리 창 하나만 생성
static bool ProcwinOpened = false;  // 프로세스 창 상태(열림/닫힘)

int initProcWin(void) {
    int y, x, h, w;

    // 위치 계산
    if (calculateProcWinPos(&y, &x, &h, &w) == -1) {
        return -1;
    }

    // 창 생성
    procWindow.win = newwin(h, w, y, x);
    if (procWindow.win == NULL) {
        return -1;
    }

    // 변수 설정
    procWindow.currentPos = 0;
    procWindow.totalReadItems = 0;
    procWindow.lineMovementEvent = 0;
    pthread_mutex_init(&procWindow.statMutex, NULL);

    ProcwinOpened = true;
    return 0;
}

void closeProcWin(void) {
    delwin(procWindow.win);  // 창 삭제
    ProcwinOpened = false;
}

void toggleProcWin(void) {
    if (ProcwinOpened) {
        closeProcWin();
    } else {
        initProcWin();
    }
}

bool checkProcWin(void){
    return ProcwinOpened;
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

int updateProcWin(void) {
    int ret, winH, winW;
    int lineMovement;
    int centerLine, currentLine;
    int itemsToPrint;
    ssize_t itemsCnt;
    size_t startIdx;

    // 프로세스 창 업데이트
    ret = pthread_mutex_trylock(&procWindow.statMutex);
    if (ret != 0)
        return -1;

    itemsCnt = procWindow.totalReadItems;  // 읽어들인 개수 가져옴

    // 선택 항목 이동 처리
    while (procWindow.lineMovementEvent) {
        lineMovement = procWindow.lineMovementEvent & 0x03;
        procWindow.lineMovementEvent >>= 2;
        switch (lineMovement) {
            case 0x02:  // '한 칸 위로 이동' Event
                if (procWindow.currentPos > 0)
                    procWindow.currentPos--;
                break;
            case 0x03:  // '한 칸 아래로 이동' Event
                if (procWindow.currentPos < itemsCnt - 1)
                    procWindow.currentPos++;
                break;
        }
    }

    // 최상단에 출력될 항목의 index 계산
    getmaxyx(procWindow.win, winH, winW);

    centerLine = (winH - 1) / 2;  // 창의 가운데 줄 번호 계산
    if (itemsCnt <= winH) {  // 항목 개수가 창 높이보다 적을 때
        startIdx = 0;
        itemsToPrint = itemsCnt;
    } else if (procWindow.currentPos < centerLine) {  // 선택된 항목이 창 위쪽에 위치할 때
        startIdx = 0;
        itemsToPrint = itemsCnt < winH ? itemsCnt : winH;
    } else if (procWindow.currentPos + (centerLine - winH - 1) >= itemsCnt) {  // 선택 항목이 창 아래쪽에 위치할 때
        startIdx = itemsCnt - winH;
        itemsToPrint = winH;
    } else {  // 일반적인 경우
        startIdx = procWindow.currentPos - centerLine;
        itemsToPrint = winH;
    }

    // 출력
    currentLine = procWindow.currentPos - startIdx;  // 역상으로 출력할, 현재 선택된 줄
    for (int line = 0; line < itemsToPrint; line++) {
        if (line == currentLine)  // 선택된 것 역상으로 출력
            wattron(procWindow.win, A_REVERSE);

        mvwprintw(procWindow.win, line, 0, "%d %s",
                  procWindow.procEntries[startIdx + line].pid,  // pid
                  procWindow.procEntries[startIdx + line].name);  // 프로세스명

        if (line == currentLine)
            wattroff(procWindow.win, A_REVERSE);
    }


    wclrtobot(procWindow.win);  // 아래 남는 공간: 지움
    wrefresh(procWindow.win);  // 창 새로 그림

    pthread_mutex_unlock(&procWindow.statMutex);
    return 0;
}
