#include <assert.h>
#include <curses.h>
#include <panel.h>
#include <stdlib.h>

#include "bottom_area.h"
#include "commons.h"
#include "config.h"
#include "file_operator.h"


/**
 * 진행률 정보 표시
 *
 * @param infos 파일 작업 진행률 정보
 * @param width 최대 출력 너비
 * @return 실행 중인 작업 수
 */
static int displayProgress(FileProgressInfo *infos, int width);

/**
 * 매뉴얼 텍스트 표시
 *
 * 대괄호([])로 묶인 부분은 역상으로 표시
 * @param width 최대 출력 너비
 */
static void displayManual(int width);

/**
 * 바텀 박스의 사이즈를 설정함
 * 이전 크기와 비교하여, 다르면, 재설정
 * @param width 최대 출력 너비의 포인터
 */
static void setBottomBoxSize(int *width);


static WINDOW *bottomBox;
static PANEL *bottomPanel;

WINDOW *initBottomBox(int width, int startY) {
    assert((bottomBox = newwin(3, width, startY, 0)));
    assert((bottomPanel = new_panel(bottomBox)));
    return bottomBox;
}

void delBottomBox(void) {
    assert((del_panel(bottomPanel) != ERR));
    assert((delwin(bottomBox) != ERR));
}

void displayManual(int width) {
    const char *manual1, *manual2;

    // 창 크기에 따라 출력 내용 결정
    if (width >= 120) {  // 전체 출력 7열
        manual1 = "[^ / v] Move     [c / x] Copy / Cut   [F2] Rename         [Delete] Delete   [p] Process   [w] NameSort   [e] SizeSort";
        manual2 = "[< / >] Switch   [  v  ] Paste        [F4] Move to Path   [Enter ] Open     [q] Quit      [r] DateSort";
    } else if (width >= 105) {  // 6열
        manual1 = "[^ / v] Move     [c / x] Copy / Cut   [F2] Rename         [Delete] Delete   [p] Process   [w] NameSort";
        manual2 = "[< / >] Switch   [  v  ] Paste        [F4] Move to Path   [Enter ] Open     [q] Quit      [r] DateSort";
    } else if (width >= 90) {  // 5열
        manual1 = "[^ / v] Move     [c / x] Copy / Cut   [F2] Rename         [Delete] Delete   [p] Process";
        manual2 = "[< / >] Switch   [  v  ] Paste        [F4] Move to Path   [Enter ] Open     [q] Quit";
    } else if (width >= 75) {  // 4열
        manual1 = "[^ / v] Move     [c / x] Copy / Cut   [F2] Rename         [Delete] Delete";
        manual2 = "[< / >] Switch   [  v  ] Paste        [F4] Move to Path   [Enter ] Open";
    } else if (width >= 60) {  // 3열
        manual1 = "[^ / v] Move     [c / x] Copy / Cut   [F2] Rename";
        manual2 = "[< / >] Switch   [  v  ] Paste        [F4] Move to Path";
    } else {  // 최소 출력 2열
        manual1 = "[^ / v] Move     [c / x] Copy / Cut";
        manual2 = "[< / >] Switch   [  v  ] Paste";
    }

    // 첫 번째 줄 출력
    wmove(bottomBox, 1, 1);
    for (int i = 0; manual1[i] != '\0' && i < width - 2; i++) {
        if (manual1[i] == '[') {
            wattron(bottomBox, A_REVERSE);
        }
        waddch(bottomBox, manual1[i]);
        if (manual1[i] == ']') {
            wattroff(bottomBox, A_REVERSE);
        }
    }

    // 두 번째 줄 출력
    wmove(bottomBox, 2, 1);
    for (int i = 0; manual2[i] != '\0' && i < width - 2; i++) {
        if (manual2[i] == '[') {
            wattron(bottomBox, A_REVERSE);
        }
        waddch(bottomBox, manual2[i]);
        if (manual2[i] == ']') {
            wattroff(bottomBox, A_REVERSE);
        }
    }
    mvwhline(bottomBox, 0, 0, ACS_HLINE, getmaxx(bottomBox));  // 바텀박스 위 가로줄
}

int displayProgress(FileProgressInfo *infos, int width) {
    int x, y, w = width / 2 - 1;
    char operation;

    int runningWins = 0;
    for (int i = 0; i < MAX_FILE_OPERATORS; i++) {
        pthread_mutex_lock(&infos[i].flagMutex);
        switch (infos[i].flags & PROGRESS_BITS) {
            case PROGRESS_COPY:
                operation = 'C';
                runningWins++;
                break;
            case PROGRESS_MOVE:
                operation = 'M';
                runningWins++;
                break;
            case PROGRESS_DELETE:
                operation = 'D';
                runningWins++;
                break;
            default:
                pthread_mutex_unlock(&infos[i].flagMutex);
                continue;
        }
        switch (runningWins) {
            case 1:
                x = 1;
                y = 1;
                break;
            case 2:
                x = width / 2 + 1;
                y = 1;
                break;
            case 3:
                x = 1;
                y = 2;
                break;
            case 4:
                x = width / 2 + 1;
                y = 2;
                break;
        }
        mvwprintw(
            bottomBox, y, x, "%c %*.*s %.3d%%",
            operation, w - 7, w - 7, infos[i].name,
            (infos[i].flags & PROGRESS_PERCENT_MASK) >> PROGRESS_PERCENT_START
        );
        pthread_mutex_unlock(&infos[i].flagMutex);
    }

    return runningWins;
}

void setBottomBoxSize(int *width) {
    static int prevScreenH = 0, prevScreenW = 0;
    int screenH, screenW;
    // 현재 화면 크기 가져오기
    getmaxyx(stdscr, screenH, screenW);
    *width = getmaxx(bottomBox);

    if (prevScreenH != screenH || prevScreenW != screenW) {
        // 최신화
        prevScreenH = screenH;
        prevScreenW = screenW;

        // 윈도우, 패널 레이아웃 재배치
        wresize(bottomBox, 3, screenW);  // 높이 3, 너비 stdscr 사이즈 윈도우 생성
        replace_panel(bottomPanel, bottomBox);  // 교체
        move_panel(bottomPanel, screenH - 3, 0);  // 옮기기
        top_panel(bottomPanel);  // 다른 창에 가려지는 거 방지

        *width = screenW;
    }
}

void updateBottomBox(FileProgressInfo *infos) {
    int width;
    setBottomBoxSize(&width);
    werase(bottomBox);
    if (displayProgress(infos, width) == 0)
        displayManual(width);
}
