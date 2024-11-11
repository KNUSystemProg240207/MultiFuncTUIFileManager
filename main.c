#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "commons.h"
#include "dir_window.h"
#include "bottom_area.h"
#include "dir_listener.h"
#include "title_bar.h"


WINDOW *titleBar, *bottomBox;

// 각 (폴더의 내용 가져오는) Directory Listener Thread별 저장 공간:
// Thread 번호 저장
static pthread_t threadListDir[MAX_DIRWINS];
// 결과 Buffer
static struct stat statEntries[MAX_DIRWINS][MAX_STAT_ENTRIES];
static char entryNames[MAX_DIRWINS][MAX_STAT_ENTRIES][MAX_NAME_LEN + 1];
static size_t totalReadItems[MAX_DIRWINS] = { 0 };
// 상태 관련
static uint16_t threadStatusFlags[MAX_DIRWINS];
static size_t chdirIdx[MAX_DIRWINS];
// Mutexes
static pthread_mutex_t statMutex[MAX_DIRWINS];
static pthread_mutex_t statusMutex[MAX_DIRWINS];
// Condition Variables
static pthread_cond_t condResumeThread[MAX_DIRWINS];

static unsigned int dirWinCnt;  // 표시된 폴더 표시 창 수

static void initVariables(void);  // 변수들 초기화
static void initScreen(void);  // ncurses 관련 초기화 & subwindow들 생성
static void initThreads(void);  // thread 관련 초기화
static void mainLoop(void);  // 프로그램 Main Loop
static void cleanup(void);  // atexit()에 전달할 함수: 프로그램 종료 직전, main() 반환 직후에 수행됨


int main(int argc, char **argv) {
    atexit(cleanup);

    initVariables();
    initScreen();
    initThreads();
    mainLoop();

    // Thread들 정지 요청
    for (int i = 0; i < dirWinCnt; i++) {
        pthread_mutex_lock(&statusMutex[i]);
        threadStatusFlags[i] = LTHREAD_FLAG_STOP;
        pthread_cond_signal(&condResumeThread[i]);
        pthread_mutex_unlock(&statusMutex[i]);
    }
    // 각 Thread들 대기
    for (int i = 0; i < dirWinCnt; i++) {
        pthread_mutex_lock(&statusMutex[i]);
        if (threadStatusFlags[i] & LTHREAD_FLAG_RUNNING) {
            pthread_mutex_unlock(&statusMutex[i]);
            pthread_join(threadListDir[i], NULL);
        }
        pthread_mutex_unlock(&statusMutex[i]);
    }

    // 창 '지움' (자원 해제)
    CHECK_CURSES(delwin(titleBar));
    CHECK_CURSES(delwin(bottomBox));

    return 0;
}

void initVariables(void) {
    // 변수들 기본값으로 초기화
    for (int i = 0; i < MAX_DIRWINS; i++) {
        pthread_mutex_init(statMutex + i, NULL);
        pthread_mutex_init(statusMutex + i, NULL);
        pthread_cond_init(condResumeThread + i, NULL);
    }
}

void initScreen(void) {
    // ncurses 초기화
    if (initscr() == NULL) {
        fprintf(stderr, "Window initialization FAILED\n");
        exit(1);
    }
    CHECK_CURSES(cbreak());  // stdin의 line buffering 해제: 키 입력 즉시 받음 (~ICANON과 유사)
    CHECK_CURSES(noecho());  // echo 끄기: 키 입력 안 보임
    CHECK_CURSES(nodelay(stdscr, TRUE));
    CHECK_CURSES(keypad(stdscr, TRUE));  // 특수 키를 일반 키처럼 입력받을 수 있게 함
    CHECK_CURSES(curs_set(0));  // 커서 숨김

    CHECK_FALSE1(has_colors(), "Doesn't support color\n", 1);  // Color 지원 검사
    CHECK_CURSES(start_color());  // Color 시작
    if (can_change_color() == TRUE) {
        CHECK_CURSES(init_color(COLOR_WHITE, 1000, 1000, 1000));  // 흰색을 '진짜' 흰색으로: 일부 환경에서, COLOR_WHITE가 회색인 경우 있음
    }

    // 창 크기 가져옴
    int h, w;
    getmaxyx(stdscr, h, w);

    titleBar = initTitleBar(w);  // 제목 창 (프로그램 이름 - 현재 경로 - 현재 시간) 생성
    bottomBox = initBottomBox(w, h - 2);  // 아래쪽 단축키 창 생성
    CHECK_CURSES(mvhline(1, 0, ACS_HLINE, w));  // 제목 창 아래로 가로줄 그림
    CHECK_CURSES(mvhline(h - 3, 0, ACS_HLINE, w));  // 단축키 창 위로 가로줄 그림

    initDirWin(&statMutex[0], statEntries[0], entryNames[0], &totalReadItems[0]);  // 폴더 내용 표시 창 생성
    dirWinCnt = 1;
}

void initThreads(void) {
    startDirListender(
        &threadListDir[0], &threadStatusFlags[0], &chdirIdx[0],
        statEntries[0], entryNames[0], &totalReadItems[0], MAX_STAT_ENTRIES,
        &statMutex[0], &statusMutex[0], &condResumeThread[0]
    );  // Directory Listener Thread 시작
}

void mainLoop(void) {
    struct timespec startTime;
    uint64_t elapsedUSec;
    char cwdBuf[MAX_CWD_LEN];
    ssize_t currentSelection;
    unsigned int currentWindow;

    char *cwd;
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &startTime);  // 시작 시간

        // 키 입력 처리
        for (int ch = wgetch(stdscr); ch != ERR; ch = wgetch(stdscr)) {
            switch (ch) {
                case KEY_UP:
                    moveCursorUp();
                    break;
                case KEY_DOWN:
                    moveCursorDown();
                    break;
                case KEY_LEFT:
                    selectNextWindow();
                    break;
                case KEY_RIGHT:
                    selectPreviousWindow();
                    break;
                // case 'n':
                // case 'N':
                case '\n':
                case KEY_ENTER:
                    currentSelection = getCurrentSelectedDirectory();
                    if (currentSelection >= 0) {
                        currentWindow = getCurrentWindow();
                        pthread_mutex_lock(&statusMutex[currentWindow]);
                        chdirIdx[currentWindow] = currentSelection;
                        threadStatusFlags[currentWindow] |= LTHREAD_FLAG_CHANGE_DIR;
                        pthread_mutex_unlock(&statusMutex[currentWindow]);
                    }
                    break;
                case 'q':
                case 'Q':
                    goto CLEANUP;  // Main Loop 빠져나감
            }
        }

        // 제목 영역 업데이트
        renderTime();  // 시간 업데이트
        // TODO: get current window's cwd
        cwd = getcwd(cwdBuf, MAX_CWD_LEN);  // 경로 가져옴
        if (cwd == NULL)
            printPath("-----");  // 경로 가져오기 실패 시
        else
            printPath(cwd);  // 경로 업데이트

        updateDirWins();  // 폴더 표시 창들 업데이트

        // 필요한 창들 refresh
        CHECK_CURSES(wrefresh(titleBar));
        CHECK_CURSES(wrefresh(bottomBox));
        refresh();

        // Frame rate 제한: delay - 경과 time
        elapsedUSec = getElapsedTime(startTime);
        if (FRAME_INTERVAL_USEC > elapsedUSec) {
            usleep(FRAME_INTERVAL_USEC - elapsedUSec);  // Main thread 일시정지
        }
    }
CLEANUP:
    return;
}

void cleanup(void) {
    endwin();
}
