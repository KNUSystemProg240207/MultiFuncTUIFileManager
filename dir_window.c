#include <curses.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>

#include "config.h"
#include "dir_window.h"


/**
 * @struct _DirWin
 * Directory Window의 정보 저장
 * 
 * @var _DirWin::win WINDOW 구조체
 * @var _DirWin::order Directory 창 순서 (가장 왼쪽=0) ( [0, MAX_DIRWINS) )
 * @var _DirWin::currentPos 현재 선택된 Element
 * @var _DirWin::statMutex 현 폴더 항목들의 Stat 및 이름 관련 Mutex
 * @var _DirWin::statEntries 항목들의 stat 정보
 * @var _DirWin::entryNames 항목들의 이름
 * @var _DirWin::totalReadItems 현 폴더에서 읽어들인 항목 수
 */
struct _DirWin {
    WINDOW *win;
    unsigned int order;
    size_t currentPos;
    pthread_mutex_t *statMutex;
    struct stat *statEntries;
    char (*entryNames)[MAX_NAME_LEN + 1];
    size_t *totalReadItems;
};
typedef struct _DirWin DirWin;


static DirWin windows[MAX_DIRWINS];  // 각 창의 runtime 정보 저장
static unsigned int winCnt;  // 창 개수


/**
 * 창 위치 계산
 * 
 * @param winNo 창의 순서 (가장 왼쪽=0) ( [0, winCnt) )
 * @param y (반환) 창 좌상단 y좌표
 * @param x (반환) 창 좌상단 x좌표
 * @param h (반환) 창 높이
 * @param w (반환) 창 폭
 * @return 성공: 0, 부적절한 인자 전달한 경우: -1
 */
static int calculateWinPos(unsigned int winNo, int *y, int *x, int *h, int *w);


int initDirWin(
    pthread_mutex_t *statMutex,
    struct stat *statEntries,
    char (*entryNames)[MAX_NAME_LEN + 1],
    size_t *totalReadItems
) {
    int y, x, h, w;
    winCnt++;
    // 위치 계산
    if (calculateWinPos(winCnt - 1, &y, &x, &h, &w) == -1) {
        return -1;
    }
    // 창 생성
    WINDOW *newWin = newwin(h, w, y, x);
    if (newWin == NULL) {
        winCnt--;
        return -1;
    }
    // 변수 설정
    windows[winCnt - 1] = (DirWin) {
        .win = newWin,
        .order = winCnt - 1,
        .currentPos = 0,
        .statMutex = statMutex,
        .statEntries = statEntries,
        .entryNames = entryNames,
        .totalReadItems = totalReadItems,
    };
    return winCnt;
}

int updateDirWins(void) {
    int ret, winH, winW, linesTopToCenter;
    size_t itemsCnt;
    ssize_t startIdx, endIdx;
    DirWin *win;

    // 각 창들 업데이트
    for (int i = 0; i < winCnt; i++) {
        win = windows + i;

        // Mutex 획득 시도: 실패 시, 파일 정보 업데이트 중: 해당 창 업데이트 건너뜀
        ret = pthread_mutex_trylock(win->statMutex);
        if (ret != 0)
            continue;
        itemsCnt = *win->totalReadItems;  // 읽어들인 개수 가져옴

        // 최상단에 출력될 항목의 index 계산
        getmaxyx(win->win, winH, winW);
        linesTopToCenter = (winW - 1) / 2;
        startIdx = win->currentPos - linesTopToCenter;
        endIdx = startIdx + winW - 1;

        if (startIdx < 0) {  // 최상단에 출력될 index가 범위 벗어난 경우
            startIdx = 0;
        } else if (itemsCnt >= endIdx) {  // 최하단에 출력될 index가 범위 벗어난 경우
            startIdx -= itemsCnt - endIdx + 1;
            endIdx = itemsCnt - 1;
        }

        // 출력
        int line;
        for (line = 0; line < winH && line < itemsCnt; line++) { // 항목 있는 공간: 출력
            mvwaddstr(win->win, line, 0, win->entryNames[startIdx + line]);  // 항목 이름 출력
            wclrtoeol(win->win);  // 현재 줄의 남은 공간: 지움
        }
        wclrtobot(win->win);  // 아래 남는 공간: 지움
        wrefresh(win->win);  // 창 새로 그림

        pthread_mutex_unlock(win->statMutex);
    }

    return 0;
}

int calculateWinPos(unsigned int winNo, int *y, int *x, int *h, int *w) {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);
    // 창의 개수에 따라 분기: 1개 (TODO: 2~3개)
    switch (winCnt) {
        case 1:  // 창 1개 존재
            if (winNo != 0)
                return -1;
            *y = 2;
            *x = 0;
            *h = screenH - 5;
            *w = screenW;
            return 0;
        // TODO: Implement below cases
        // case 2:
        //     return 0;
        // case 3:
        //     return 0;
        default:
            return -1;
    }
}

