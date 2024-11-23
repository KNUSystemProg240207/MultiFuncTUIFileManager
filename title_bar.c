#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "commons.h"
#include "config.h"
#include "title_bar.h"


static WINDOW *titleBar;  // 제목 Bar Window
static unsigned int barWidth;  // 제목 창 너비
static unsigned int margin;  // 현재 경로 양 옆의 공간 (프로그램명 및 시간 출력용)


WINDOW *initTitleBar(int width) {
    CHECK_NULL(titleBar = newwin(2, width, 0, 0));  // 새 Window 생성
    CHECK_CURSES(mvwaddstr(titleBar, 0, 0, PROG_NAME));  // 프로그램 이름 출력

    barWidth = width;
    margin = PROG_NAME_LEN > DATETIME_LEN ? PROG_NAME_LEN + 1 : DATETIME_LEN + 1;  // 양 옆 공간 계산

    return titleBar;
}

void renderTime() {
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

void printPath(char *path, size_t pathLen) {
    int size = barWidth - margin * 2;  // 제목 영역 크기
    int restSpace = size - pathLen;  // 제목 표시된 후 남는 공간
    int start = margin + (restSpace > 0 ? restSpace / 2 : 0);
    mvwhline(titleBar, 0, margin, ' ', size);  // 공백 출력 -> 기존 내용 삭제 (기존보다 더 짧은 문자열 고려)
    mvwaddnstr(titleBar, 0, start, path, size < pathLen ? size : pathLen);  // 제목 표시: 가운데 정렬 (공간 부족하면: 끝부분이 잘림)
}

void updateTitleBar(char *cwd, size_t cwdLen) {
    int width = getmaxx(stdscr);
    int requiredWidthPartial;
    int size;

    // cwd가 NULL이거나 cwdLen이 비정상적인 경우 처리
    if (cwd == NULL || cwdLen == (size_t)-1) {
        cwd = "-----";  // 상수 문자열로 기본 값 설정
        cwdLen = strlen(cwd);  // 길이 재설정
    } else {
        cwd[MAX_PATH_LEN - 1] = '\0';
    }

    werase(titleBar);  // 기존 내용을 지움

    renderMargin(width);  // margin 계산
    requiredWidthPartial = DATETIME_LEN + cwdLen;  // 경로와 시간을 출력하는 데 필요한 너비
    size = barWidth - margin * 2;  // 제목 영역 크기 계산

    if (size >= cwdLen) {
        // 충분한 너비: 프로그램 이름, 경로, 시간 모두 출력
        printProgramName();  // 왼쪽에 프로그램 이름 출력
        printPath(cwd, cwdLen);
        renderTime();  // 오른쪽 끝에 시간 출력
    } else if (width >= requiredWidthPartial) {
        // 중간 너비: 경로와 시간만 출력
        mvwaddnstr(titleBar, 0, 0, cwd, cwdLen);
        renderTime();  // 오른쪽 끝에 시간 출력
    } else {
        // 최소 너비: 경로만 출력
        mvwaddnstr(titleBar, 0, 0, cwd, cwdLen);
    }
    mvwhline(titleBar, 1, 0, ACS_HLINE, getmaxx(stdscr));
}


void renderMargin(int width) {
    barWidth = width;
    margin = PROG_NAME_LEN > DATETIME_LEN ? PROG_NAME_LEN + 1 : DATETIME_LEN + 1;  // 양 옆 공간 계산
}

void printProgramName() {
    CHECK_CURSES(mvwaddstr(titleBar, 0, 0, PROG_NAME));  // 프로그램 이름 출력
}