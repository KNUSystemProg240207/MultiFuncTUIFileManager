#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "commons.h"
#include "title_bar.h"


static WINDOW *titleBar;  // 제목 Bar Window
static unsigned int barWidth;  // 제목 창 너비
static unsigned int margin;  // 현재 경로 양 옆의 공간 (프로그램명 및 시간 출력용)


WINDOW *initTitleBar(int width) {
    CHECK_NULL(titleBar = newwin(1, width, 0, 0));  // 새 Window 생성
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

void printPath(char *path) {
    int size = barWidth - margin * 2;  // 제목 영역 크기
    int restSpace = size - strlen(path);  // 제목 표시된 후 남는 공간
    mvwaddnstr(titleBar, 0, margin + (restSpace > 0 ? restSpace / 2 : 0), path, size);  // 제목 표시: 가운데 정렬 (공간 부족하면: 끝부분이 잘림)
}
