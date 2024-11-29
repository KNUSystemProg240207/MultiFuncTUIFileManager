#include <assert.h>
#include <curses.h>
#include <panel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commons.h"
#include "config.h"
#include "title_bar.h"


static WINDOW *titleBar;  // 제목 Bar Window
static PANEL *titlePanel;


/**
 * 화면 우상단에 현재 시간 표시 (또는 새로고침)
 */
static void renderTime(int barWidth);

/**
 * 화면 상단 가운데에 경로 출력
 *
 * @param path 출력할 경로 문자열
 * @param pathLen (NULL-문자 제외한) 경로 문자열의 길이
 */
static void printPath(char *path, size_t pathLen, int barWidth);


WINDOW *initTitleBar(int width) {
    assert((titleBar = newwin(2, width, 0, 0)));
    assert((titlePanel = new_panel(titleBar)));
    CHECK_CURSES(mvwaddstr(titleBar, 0, 0, PROG_NAME));  // 프로그램 이름 출력

    return titleBar;
}

void delTitleBar(void) {
    assert((del_panel(titlePanel) != ERR));
    assert((delwin(titleBar) != ERR));
}

void renderTime(int barWidth) {
    static char buf[DATETIME_LEN + 1];  // 결과값 저장 Buffer
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) == -1) {  // 현재 시간 (Wall-Clock Time) 가져옴
        perror("Time");
        exit(-1);
    }
    struct tm *dt = localtime(&now.tv_sec);  // 가져온 시간을 구조체로 변환
    DT_TO_STR(buf, dt);  // 날짜 및 시간 Formatting
    mvwaddstr(titleBar, 0, barWidth - DATETIME_LEN, buf);  // 출력
}

void printPath(char *path, size_t pathLen, int barWidth) {
    int restSpace = barWidth - pathLen;  // 제목 표시된 후 남는 공간
    int start = (restSpace > 0 ? restSpace / 2 : 0);  // 시작 X좌표
    mvwaddnstr(titleBar, 0, start, path, barWidth < pathLen ? barWidth : pathLen);  // 제목 표시: 가운데 정렬 (공간 부족하면: 끝부분이 잘림)
}

void updateTitleBar(char *cwd, size_t cwdLen) {
    werase(titleBar);  // 기존 내용을 지움
    int screenWidth = getmaxx(stdscr);
    int longerLen = PROG_NAME_LEN > DATETIME_LEN ? PROG_NAME_LEN : DATETIME_LEN;

    if (screenWidth >= cwdLen + longerLen * 2 + 2) {  // 프로그램 이름 + 경로 + 시간
        CHECK_CURSES(mvwaddstr(titleBar, 0, 0, PROG_NAME));  // 프로그램 이름 (왼쪽)
        printPath(cwd, cwdLen, screenWidth);  // 현재 경로 (가운데)
        renderTime(screenWidth);  // 시간 (오른쪽)
    } else if (screenWidth >= cwdLen + DATETIME_LEN + 1) {  // 경로 + 시간
        mvwaddnstr(titleBar, 0, 0, cwd, cwdLen);  // 현재 경로 (왼쪽)
        renderTime(screenWidth);  // 오른쪽 끝에 시간 출력
    } else {  // 경로만
        mvwaddnstr(titleBar, 0, 0, cwd, cwdLen);  // 현재 경로
    }
    mvwhline(titleBar, 1, 0, ACS_HLINE, getmaxx(stdscr));
}
