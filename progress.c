#include <ncurses.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "progress.h"

#define MAX_NAME_LEN 256
#define MAX_THREADS 4

#define PROGRESS_COPY (1 << 10)
#define PROGRESS_MOVE (2 << 10)
#define PROGRESS_DELETE (3 << 10)
#define PROGRESS_PERCENT_START 1
#define PROGRESS_PERCENT_MASK (0x7f << PROGRESS_PERCENT_START)

typedef struct _FileProgressInfo {
    char name[MAX_NAME_LEN];  // 작업중인 파일 이름
    uint16_t flags;  // 진행률 Bit Field
    pthread_mutex_t flagMutex;  // 진행률 보호 Mutex
} FileProgressInfo;
FileProgressInfo info[MAX_THREADS];

// 진행률 계산 함수
int CalculProgress(FileProgressInfo info) {
    int percent;
    percent = info.flags & PROGRESS_PERCENT_MASK;
    return percent;
}

// 진행률 표시 함수
void displayProgress(WINDOW *bottomBox, int startY) {
    int width, height;
    getmaxyx(bottomBox, height, width);  // bottomBox의 너비와 높이 가져오기

    int left_x = 1, right_x = width / 2 + 1;
    int top_y = startY, bottom_y = height / 2 + startY;

    for (int i = 0; i < MAX_THREADS; i++) {
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

        pthread_mutex_lock(&info[i].flagMutex);

        mvwprintw(bottomBox, y, x, "File: %s", info[i].name);
        mvwprintw(bottomBox, y + 1, x, "Progress: %d%%", CalculProgress(info[i]));

        if (info[i].flags & PROGRESS_COPY) {
            mvwprintw(bottomBox, y + 2, x, "Status: Copying");
        } else if (info[i].flags & PROGRESS_MOVE) {
            mvwprintw(bottomBox, y + 2, x, "Status: Moving");
        } else if (info[i].flags & PROGRESS_DELETE) {
            mvwprintw(bottomBox, y + 2, x, "Status: Deleting");
        }

        mvwprintw(bottomBox, y + 3, x, "-----------------");
        pthread_mutex_unlock(&info[i].flagMutex);
    }

    wrefresh(bottomBox);
}