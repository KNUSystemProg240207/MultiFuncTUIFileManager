#include <limits.h>
#include <panel.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
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

static uint64_t lineMovementEvent = 0;  // 커서 이동 이벤트 저장
static size_t currentPos = 0;


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
static void printProcessInfo(WINDOW *win, int winW, Process process, int line);

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

static void processLineMovementEvent(void);


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
    int availableH;
    int centerLine;
    int itemsToPrint;
    size_t startIdx;

    setProcessWinSize(&winH, &winW);
    werase(window);
    box(window, 0, 0);

    // 프로세스 창 업데이트
    pthread_mutex_lock(bufMutex);
    processLineMovementEvent();

    size_t itemsCnt = *totalReadItems;  // 읽어들인 총 항목 수
    int maxItemsToPrint = winH - 3;  // 상하단 여백 제외 창 높이에 비례한 출력 가능한 항목 수
    if (maxItemsToPrint > itemsCnt)
        maxItemsToPrint = itemsCnt;

    availableH = winH - 3;  // 최대 출력 가능한 라인 넘버 -3

    // 라인 스크롤
    centerLine = (availableH - 1) / 2;  // 가운데 줄의 줄 번호 ( [0, availableH) )
    if (itemsCnt <= availableH) {  // 항목 개수 적음 -> 빠르게 처리
        startIdx = itemsCnt - 1;  // 가장 마지막 데이터부터 출력
        itemsToPrint = itemsCnt;
    } else if (currentPos < centerLine) {  // 위쪽 item 선택됨
        startIdx = itemsCnt - 1;  // 가장 마지막 데이터부터 출력
        itemsToPrint = availableH;  // 창 크기만큼 출력
    } else if (currentPos >= itemsCnt - (availableH - centerLine - 1)) {  // 아래쪽 item 선택된 경우 (우변: 개수 - 커서 아래쪽에 출력될 item 수)
        startIdx = availableH - 1;  // 출력할 데이터의 마지막 항목으로 설정
        itemsToPrint = availableH;
    } else {  // 일반적인 경우
        startIdx = itemsCnt - currentPos - 1 + centerLine;  // currentPos 기준으로 중앙 정렬
        itemsToPrint = availableH;
    }

    if (isColorSafe)
    wbkgd(window, COLOR_PAIR(PRCSBGRND));
    printTableHeader(window, winW);  // 테이블 헤더 출력
    applyColor(window, PRCSFILE);  // 색상 적용

    for (int i = 0; i < itemsToPrint; i++) {
        size_t processIndex = startIdx - i;  // 화면 출력 시작 인덱스 기준으로 출력
        if (processIndex >= itemsCnt)  // 데이터 범위 초과 방지
            break;
        if (processIndex == itemsCnt - 1 - currentPos)  // 현재 선택된 프로세스 강조
            wattron(window, A_REVERSE);
        printProcessInfo(window, winW, processes[processIndex], i + 2);  // 단일 프로세스 정보 출력
        if (processIndex == itemsCnt - 1 - currentPos)
            wattroff(window, A_REVERSE);
    }
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
void printProcessInfo(WINDOW *win, int winW, Process process, int line) {
    if (winW < 20) {
        mvwprintw(win, line, 1, "%6d", process.pid);
    } else if (winW < 30) {
        mvwprintw(win, line, 1, "%6d %14s", process.pid, formatSize(process.rsize));
    } else if (winW < 40) {
        mvwprintw(win, line, 1, "%6d %14s %9lus", process.pid, formatSize(process.rsize), ticksToSeconds(process.utime));
    } else if (winW < 50) {
        mvwprintw(win, line, 1, "%6d %14s %9lus %9lus", process.pid, formatSize(process.rsize), ticksToSeconds(process.utime), ticksToSeconds(process.stime));
    } else if (winW < 80) {
        mvwprintw(win, line, 1, "%6d %6c %14s %9lus %9lus", process.pid, process.state, formatSize(process.rsize), ticksToSeconds(process.utime), ticksToSeconds(process.stime));
    } else {
        mvwprintw(win, line, 1, "%6d %-*.*s %6c %14s %9lus %9lus", process.pid, winW - 55, winW - 55, process.name, process.state, formatSize(process.rsize), ticksToSeconds(process.utime), ticksToSeconds(process.stime));
    }
}

void selectPreviousProcess() {
    // 이미 공간 다 썼다면 (= MSB가 1) -> 이후 수신된 이벤트 버림
    if (lineMovementEvent & (UINT64_C(0x80) << (7 * 8)))
        return;
    lineMovementEvent = lineMovementEvent << 2 | 0x02;  // <한 칸 위로> Event 저장
}

void selectNextProcess() {
    // 이미 공간 다 썼다면 (= MSB가 1) -> 이후 수신된 이벤트 버림
    if (lineMovementEvent & (UINT64_C(0x80) << (7 * 8)))
        return;
    lineMovementEvent = lineMovementEvent << 2 | 0x03;  // <한 칸 아래로> Event 저장
}

void getSelectedProcess(pid_t *pid, char *nameBuf, size_t bufLen) {
    pthread_mutex_lock(bufMutex);
    processLineMovementEvent();
    int killIndex = *totalReadItems - currentPos - 1;
    *pid = processes[killIndex].pid;
    strncpy(nameBuf, processes[killIndex].name, bufLen - 1);
    nameBuf[bufLen - 1] = '\0';
    pthread_mutex_unlock(bufMutex);
}

void processLineMovementEvent(void) {
    size_t itemsCnt = *totalReadItems;  // 읽어들인 총 항목 수

    int eventStartPos;
    int lineMovement;
    for (eventStartPos = 0; (lineMovementEvent & (UINT64_C(2) << eventStartPos)) != 0; eventStartPos += 2);
    while (eventStartPos > 0) {
        eventStartPos -= 2;
        lineMovement = (lineMovementEvent >> eventStartPos) & 0x03;
        switch (lineMovement) {
            case 0x02:  // '한 칸 위로 이동' Event
                if (currentPos > 0)  // 위로 이동 가능하면
                    currentPos--;
                else
                    currentPos = 0;  // Corner case 처리: 최소 Index
                break;
            case 0x03:  // '한 칸 아래로 이동' Event
                if (currentPos < itemsCnt - 1)  // 아래로 이동 가능하면
                    currentPos++;
                else
                    currentPos = itemsCnt - 1;  // Corner case 처리: 최대 Index
                break;
        }
    }
    lineMovementEvent = 0;  // 이벤트 초기화
}


// tick을 초 단위로 변경합니다.
unsigned long ticksToSeconds(unsigned long ticks) {
    return ticks / sysconf(_SC_CLK_TCK);  // 시스템 틱을 초로 변환.
}
