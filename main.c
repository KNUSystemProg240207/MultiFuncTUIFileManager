#include <assert.h>
#include <curses.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "bottom_area.h"
#include "colors.h"
#include "commons.h"
#include "config.h"
#include "dir_listener.h"
#include "dir_window.h"
#include "list_process.h"
#include "proc_win.h"
#include "title_bar.h"

WINDOW *titleBar, *bottomBox;
PANEL *titlePanel, *bottomPanel;  // 패널 추가

mode_t directoryOpenArgs;  // fdopendir()에 전달할 directory file descriptor를 open()할 때 쓸 argument: Thread 시작 전 저장되어야 함

// 각 (폴더의 내용 가져오는) Directory Listener Thread별 저장 공간:
static pthread_t threadListDir[MAX_DIRWINS];
static DirListenerArgs dirListenerArgs[MAX_DIRWINS];

static pthread_t threadProcess;  // 프로세스 스레드
static ProcWin proc_Win;
pthread_mutex_t proc_Win_Mutex;

static ProcThreadArgs procThreadArgs;  // 프로세스 스레드 시작을 위한 인자
WINDOW *p_win;
pthread_mutex_t p_statMutex;
ProcInfo p_Entries[MAX_PROCESSES];  // 프로세스 정보 배열
pthread_mutex_t p_visibleMutex;

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
        pthread_mutex_lock(&dirListenerArgs[i].commonArgs.statusMutex);
        dirListenerArgs[i].commonArgs.statusFlags |= THREAD_FLAG_STOP;
        pthread_cond_signal(&dirListenerArgs[i].commonArgs.resumeThread);
        pthread_mutex_unlock(&dirListenerArgs[i].commonArgs.statusMutex);
    }
    // 각 Thread들 대기
    for (int i = 0; i < dirWinCnt; i++) {
        pthread_mutex_lock(&dirListenerArgs[i].commonArgs.statusMutex);
        if (dirListenerArgs[i].commonArgs.statusFlags & THREAD_FLAG_RUNNING) {
            pthread_mutex_unlock(&dirListenerArgs[i].commonArgs.statusMutex);
            pthread_join(threadListDir[i], NULL);
        } else {
            pthread_mutex_unlock(&dirListenerArgs[i].commonArgs.statusMutex);
        }
    }

    // 패널들 정리
    del_panel(titlePanel);
    del_panel(bottomPanel);

    // 창 '지움' (자원 해제)
    CHECK_CURSES(delwin(titleBar));
    CHECK_CURSES(delwin(bottomBox));


    return 0;
}

void initVariables(void) {
    // 변수들 기본값으로 초기화
    for (int i = 0; i < MAX_DIRWINS; i++) {
        pthread_mutex_init(&dirListenerArgs[i].bufMutex, NULL);
        pthread_cond_init(&dirListenerArgs[i].commonArgs.resumeThread, NULL);
        pthread_mutex_init(&dirListenerArgs[i].commonArgs.statusMutex, NULL);
    }

    // `proc_Win` 구조체 초기화
    proc_Win = (ProcWin) {
        .win = p_win,
        .statMutex = PTHREAD_MUTEX_INITIALIZER,
        .visibleMutex = PTHREAD_MUTEX_INITIALIZER,
        .isWindowVisible = false,
        .totalReadItems = 0,
    };

    // 포인터 배열 초기화
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        proc_Win.procEntries[i] = &p_Entries[i];
    }

    procThreadArgs = (ProcThreadArgs) {
        .procWin = &proc_Win,
        .procWinMutex = &proc_Win_Mutex,
    };

    pthread_mutex_init(&proc_Win_Mutex, NULL);
    pthread_mutex_init(&proc_Win.visibleMutex, NULL);
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
    init_colorSet();

    // 창 크기 가져옴
    int h, w;
    getmaxyx(stdscr, h, w);

    titleBar = initTitleBar(w);  // 제목 창 (프로그램 이름 - 현재 경로 - 현재 시간) 생성
    bottomBox = initBottomBox(w, h - 2);  // 아래쪽 단축키 창 생성
    CHECK_CURSES(mvhline(1, 0, ACS_HLINE, w));  // 제목 창 아래로 가로줄 그림
    CHECK_CURSES(mvhline(h - 3, 0, ACS_HLINE, w));  // 단축키 창 위로 가로줄 그림

    // 패널 생성
    titlePanel = new_panel(titleBar);
    bottomPanel = new_panel(bottomBox);


    // 폴더 내용 표시 창 생성
    initDirWin(
        &dirListenerArgs[0].bufMutex,
        dirListenerArgs[0].statBuf,
        dirListenerArgs[0].nameBuf,
        &dirListenerArgs[0].totalReadItems
    );
    dirWinCnt = 1;
}

void initThreads(void) {
    dirListenerArgs[0].currentDir = opendir(".");
    assert(dirListenerArgs[0].currentDir != NULL);
    directoryOpenArgs = fcntl(dirfd(dirListenerArgs[0].currentDir), F_GETFL);
    assert(directoryOpenArgs != -1);
    startDirListender(&threadListDir[0], &dirListenerArgs[0]);  // Directory Listener Thread 시작

    startProcThread(&threadProcess, &procThreadArgs);  // 프로세스 스레드 시작
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
                case 'w':  // F1 키 (이름 기준 오름차순)
                    toggleSort(SORT_NAME_MASK, SORT_NAME_SHIFT);
                    break;
                case 'e':  // F2 키 (크기 기준 오름차순)
                    toggleSort(SORT_SIZE_MASK, SORT_SIZE_SHIFT);
                    break;
                case 'r':  // F3 키 (날짜 기준 오름차순)
                    toggleSort(SORT_DATE_MASK, SORT_DATE_SHIFT);
                    break;
                case '\n':
                case KEY_ENTER:
                    currentSelection = getCurrentSelectedDirectory();
                    if (currentSelection >= 0) {
                        currentWindow = getCurrentWindow();
                        pthread_mutex_lock(&dirListenerArgs[currentWindow].bufMutex);
                        dirListenerArgs[currentWindow].chdirIdx = currentSelection;
                        dirListenerArgs[currentWindow].commonArgs.statusFlags |= DIRLISTENER_FLAG_CHANGE_DIR;
                        pthread_mutex_unlock(&dirListenerArgs[currentWindow].bufMutex);
                        setCurrentSelectedDirectory(0);
                    }
                    break;
                case 'p':
                    toggleProcWin(&proc_Win);
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
            printPath(cwd);  // 경로 업데이트s

        updateDirWins();  // 폴더 표시 창들 업데이트

        pthread_mutex_lock(&proc_Win.visibleMutex);
        if (proc_Win.isWindowVisible) {
            pthread_mutex_unlock(&proc_Win.visibleMutex);  // Unlock before update
            updateProcWin(&proc_Win);
        } else {
            pthread_mutex_unlock(&proc_Win.visibleMutex);  // Unlock if not visible
        }

        // 패널 업데이트
        update_panels();
        doupdate();

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

    pthread_mutex_destroy(&proc_Win.statMutex);
    pthread_mutex_destroy(&proc_Win.visibleMutex);
    pthread_mutex_destroy(&proc_Win_Mutex);
}
