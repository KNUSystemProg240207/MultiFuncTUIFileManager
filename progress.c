#include <stdint.h>
#include <stdio.h>
#include <ncurses.h>

#include "config.h"
#include "progress.h"

#define NONE 0
#define COPYING (1 << 7)
#define MOVING (1 << 8)
#define REMOVING (1 << 9)

void updateProgress(uint16_t *prog, uint8_t percent) {
    if (percent > 100) percent = 100;
    *prog = (*prog & ~PERCENTAGE_MASK) | (percent & PERCENTAGE_MASK);  // 진행률 업데이트
}

uint8_t getPercentage(uint16_t prog) {
    return prog & PERCENTAGE_MASK;
}

void setFilename(int index, const char *fileName) {
    strncpy(currentFile[index], fileName, MAX_NAME_LEN);
}

int isCopying(uint16_t prog) { return (prog & COPYING) != 0; }
int isMoving(uint16_t prog) { return (prog & MOVING) != 0; }
int isDeleting(uint16_t prog) { return (prog & REMOVING) != 0; }

void CalculProgress(int index, uint64_t offset, uint64_t total_size) {
    uint8_t percent = (offset * 100) / total_size;  // 진행률 계산
    updateProgress(&progress[index], percent);  // progress 배열에 업데이트
}

void displayProgress(WINDOW *bottomBox, int startY) {
    int width, height;
    getmaxyx(bottomBox, height, width);  // bottomBox의 너비와 높이 가져오기

    // 각 구역의 시작 좌표 정의
    int left_x = 1, right_x = width / 2 + 1;
    int top_y = startY, bottom_y = height / 2 + startY;

    // 각 작업 스레드의 정보를 네 구역에 배치
    for (int i = 0; i < 4; i++) {
        int x, y;
        switch (i) {
            case 0:
                x = left_x;
                y = top_y;
                break;
            case 1:
                x = right_x;
                y = top_y;
                break;
            case 2:
                x = left_x;
                y = bottom_y;
                break;
            case 3:
                x = right_x;
                y = bottom_y;
        }

        mvwprintw(bottomBox, y, x, "File: %s", currentFile[i]);
        mvwprintw(bottomBox, y + 1, x, "Progress: %d%%", getPercentage(progress[i]));

        if (isCopying(progress[i])) {
            mvwprintw(bottomBox, y + 2, x, "Status: Copying");
        } else if (isMoving(progress[i])) {
            mvwprintw(bottomBox, y + 2, x, "Status: Moving");
        } else if (isDeleting(progress[i])) {
            mvwprintw(bottomBox, y + 2, x, "Status: Removing");
        }

        mvwprintw(bottomBox, y + 3, x, "-----------------");
    }

    wrefresh(bottomBox);
}
