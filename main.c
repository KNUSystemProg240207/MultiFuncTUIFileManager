#include <assert.h>
#include <curses.h>
#include <fcntl.h>
#include <panel.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "bottom_area.h"
#include "colors.h"
#include "commons.h"
#include "config.h"
#include "dir_listener.h"
#include "dir_window.h"
#include "file_operator.h"
#include "list_process.h"
#include "process_window.h"
#include "title_bar.h"

WINDOW *titleBar, *bottomBox;
PANEL *titlePanel, *bottomPanel;  // 패널 추가

mode_t directoryOpenArgs;  // fdopendir()에 전달할 directory file descriptor를 open()할 때 쓸 argument: Thread 시작 전 저장되어야 함

static pthread_t threadListDir[MAX_DIRWINS];
static pthread_t threadFileOperators[MAX_FILE_OPERATORS];
static pthread_t threadProcess;

static DirListenerArgs dirListenerArgs[MAX_DIRWINS];
static FileProgressInfo fileProgresses[MAX_FILE_OPERATORS];
static FileOperatorArgs fileOpArgs[MAX_FILE_OPERATORS];
static ProcessThreadArgs processThreadArgs;

static int pipeFileOpCmd;

pthread_mutex_t processWindow_Mutex;

pthread_mutex_t p_statMutex;
pthread_mutex_t p_visibleMutex;

static unsigned int dirWinCnt;  // 표시된 폴더 표시 창 수

static void initVariables(void);  // 변수들 초기화
static void initScreen(void);  // ncurses 관련 초기화 & subwindow들 생성
static void initThreads(void);  // thread 관련 초기화
static void mainLoop(void);  // 프로그램 Main Loop
static void stopThreads(void);  // 실행 중인 Thread들 정지
static void cleanup(void);  // atexit()에 전달할 함수: 프로그램 종료 직전, main() 반환 직후에 수행됨


int main(int argc, char **argv) {
    signal(SIGINT, SIG_IGN);
    atexit(cleanup);

    initVariables();
    initScreen();
    initThreads();
    mainLoop();

    stopThreads();

    // 창 '지움' (자원 해제)
    delProcessWindow();
    delTitleBar();
    delBottomBox();

    return 0;
}

void initVariables(void) {
    // 변수들 기본값으로 초기화
    for (int i = 0; i < MAX_DIRWINS; i++) {
        pthread_cond_init(&dirListenerArgs[i].commonArgs.resumeThread, NULL);
        pthread_mutex_init(&dirListenerArgs[i].commonArgs.statusMutex, NULL);
        pthread_mutex_init(&dirListenerArgs[i].bufMutex, NULL);
        pthread_mutex_init(&dirListenerArgs[i].dirMutex, NULL);
    }
    for (int i = 0; i < MAX_FILE_OPERATORS; i++) {
        pthread_cond_init(&fileOpArgs[i].commonArgs.resumeThread, NULL);
        pthread_mutex_init(&fileOpArgs[i].commonArgs.statusMutex, NULL);
        pthread_mutex_init(&fileOpArgs[i].pipeReadMutex, NULL);
        pthread_mutex_init(&fileProgresses[i].flagMutex, NULL);
        fileOpArgs[i].progressInfo = fileProgresses;
    }
    pthread_cond_init(&processThreadArgs.commonArgs.resumeThread, NULL);
    pthread_mutex_init(&processThreadArgs.commonArgs.statusMutex, NULL);
    pthread_mutex_init(&processThreadArgs.entriesMutex, NULL);
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
        initColorSet();
    }

    // 창 크기 가져옴
    int h, w;
    getmaxyx(stdscr, h, w);

    initTitleBar(w);  // 제목 창 (프로그램 이름 - 현재 경로 - 현재 시간) 생성
    initBottomBox(w, h - 2);  // 아래쪽 단축키 창 생성
    CHECK_CURSES(mvhline(1, 0, ACS_HLINE, w));  // 제목 창 아래로 가로줄 그림
    CHECK_CURSES(mvhline(h - 3, 0, ACS_HLINE, w));  // 단축키 창 위로 가로줄 그림

    // 폴더 내용 표시 창 생성
    initDirWin(
        &dirListenerArgs[0].bufMutex,
        &dirListenerArgs[0].totalReadItems,
        dirListenerArgs[0].dirEntries
    );
    initProcessWindow(
        &processThreadArgs.entriesMutex,
        &processThreadArgs.totalReadItems,
        processThreadArgs.processEntries
    );
    dirWinCnt = 1;
}

void initThreads(void) {
    // Directory Listener Thread 초기화, 실행
    dirListenerArgs[0].currentDir = opendir(".");
    assert(dirListenerArgs[0].currentDir != NULL);
    directoryOpenArgs = fcntl(dirfd(dirListenerArgs[0].currentDir), F_GETFL);
    assert(directoryOpenArgs != -1);
    startDirListender(&threadListDir[0], &dirListenerArgs[0]);

    // 프로세스 스레드 시작
    processThreadArgs.commonArgs.statusFlags |= LISTPROCESS_FLAG_PAUSE_THREAD;  // 초기: 일시정지 된 상태로 시작
    startProcessThread(&threadProcess, &processThreadArgs);

    // File Operator Thread 초기화, 실행
    int pipeEnds[2];
    assert(pipe(pipeEnds) == 0);
    pipeFileOpCmd = pipeEnds[1];
    for (int i = 0; i < MAX_FILE_OPERATORS; i++) {
        fileOpArgs[i].pipeEnd = pipeEnds[0];
        startFileOperator(&threadFileOperators[i], &fileOpArgs[i]);
    }
}

void mainLoop(void) {
    // clang-format off
    // clang-format on

    struct timespec startTime;  // Iteration 시작 시간
    uint64_t elapsedUSec;  // Iteration 걸린 시간

    int cwdFd;  // 현재 선택된 창의 Working Directory File Descriptor
    ssize_t cwdLen;
    // char *cwd;  // 현재 선택된 창의 Working Directory
    char fdPathBuf[MAX_PATH_LEN];  // Working Directory의 Buffer
    char cwdBuf[MAX_PATH_LEN];  // Working Directory의 Buffer

    unsigned int curWin;  // 현재 창
    ssize_t currentSelection;  // 선택된 Item
    // File Task (copy/move 및 source 정보 등 저장)
    FileTask fileTask = {
        .src.dirFd = -1,
    };
    // File Task (Delete 전용: Copy/Move 원본 지정에 영향 미치지 않게)
    FileTask fileDelTask = {
        .type = DELETE,
    };

    bool showProcessWindow = false, prevShowProcessWindow = false;

// 아래에서, dirFd 변수들에 대해 fd leak 경고 발생
// File Operator Thread에서 close() -> 여기서 close()하면, Thread쪽에서 오류 발생
// 따라서, 의도적으로 leak되게 함
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &startTime);  // 시작 시간 가져옴

        // 키 입력 처리
        for (int ch = wgetch(stdscr); ch != ERR; ch = wgetch(stdscr)) {
            switch (ch) {
                // 창 이동
                case KEY_UP:
                    moveCursorUp();
                    break;
                case KEY_DOWN:
                    moveCursorDown();
                    break;

                // 선택 항목 이동
                case KEY_LEFT:
                    selectNextWindow();
                    break;
                case KEY_RIGHT:
                    selectPreviousWindow();
                    break;

                // 디렉터리 변경
                case '\n':
                case KEY_ENTER:
                    currentSelection = getCurrentSelectedDirectory();
                    if (currentSelection >= 0) {
                        curWin = getCurrentWindow();
                        pthread_mutex_lock(&dirListenerArgs[curWin].bufMutex);
                        dirListenerArgs[curWin].chdirIdx = currentSelection;
                        dirListenerArgs[curWin].commonArgs.statusFlags |= DIRLISTENER_FLAG_CHANGE_DIR;
                        pthread_mutex_unlock(&dirListenerArgs[curWin].bufMutex);
                        pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                        pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
                        pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                        setCurrentSelection(0);
                    }
                    break;

                // 프로세스 창 토글
                case 'p':
                    showProcessWindow = !showProcessWindow;
                    break;

                // 정렬 변경
                case 'w':  // F1 키 (이름 기준 오름차순)
                    toggleSort(SORT_NAME_MASK, SORT_NAME_SHIFT);
                    curWin = getCurrentWindow();
                    pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                    // 현재 기준이 이름순인지 확인
                    if ((dirListenerArgs[curWin].commonArgs.statusFlags & SORT_CRITERION_MASK) == SORT_NAME) {
                        // 기준이 같다면 방향 토글
                        dirListenerArgs[curWin].commonArgs.statusFlags ^= SORT_DIRECTION_BIT;
                    } else {
                        // 기준이 다르면 기준 초기화 및 오름차순 설정
                        dirListenerArgs[curWin].commonArgs.statusFlags &= ~(SORT_CRITERION_MASK | SORT_DIRECTION_BIT);
                        dirListenerArgs[curWin].commonArgs.statusFlags |= SORT_NAME;
                    }
                    pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
                    pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                    break;
                case 'e':  // F2 키 (크기 기준 오름차순)
                    toggleSort(SORT_SIZE_MASK, SORT_SIZE_SHIFT);
                    curWin = getCurrentWindow();
                    pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                    // 현재 기준이 크기순인지 확인
                    if ((dirListenerArgs[curWin].commonArgs.statusFlags & SORT_CRITERION_MASK) == SORT_SIZE) {
                        // 기준이 같다면 방향 토글
                        dirListenerArgs[curWin].commonArgs.statusFlags ^= SORT_DIRECTION_BIT;
                    } else {
                        // 기준이 다르면 기준 초기화 및 오름차순 설정
                        dirListenerArgs[curWin].commonArgs.statusFlags &= ~(SORT_CRITERION_MASK | SORT_DIRECTION_BIT);
                        dirListenerArgs[curWin].commonArgs.statusFlags |= SORT_SIZE;
                    }
                    pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
                    pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                    break;
                case 'r':  // F3 키 (날짜 기준 오름차순)
                    toggleSort(SORT_DATE_MASK, SORT_DATE_SHIFT);
                    curWin = getCurrentWindow();
                    pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                    // 현재 기준이 날짜순인지 확인
                    if ((dirListenerArgs[curWin].commonArgs.statusFlags & SORT_CRITERION_MASK) == SORT_DATE) {
                        // 기준이 같다면 방향 토글
                        dirListenerArgs[curWin].commonArgs.statusFlags ^= SORT_DIRECTION_BIT;
                    } else {
                        // 기준이 다르면 기준 초기화 및 오름차순 설정
                        dirListenerArgs[curWin].commonArgs.statusFlags &= ~(SORT_CRITERION_MASK | SORT_DIRECTION_BIT);
                        dirListenerArgs[curWin].commonArgs.statusFlags |= SORT_DATE;
                    }
                    pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
                    pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                    break;

                // 복사, 잘라내기, 붙여넣기
                case 'c':
                case 'C':  // 복사
                case 'x':
                case 'X':  // 잘라내기
                    // 파일 정보 저장
                    // 기존에 Source로 선택된 Item 있었으면: 해당 dirfd close (여기서 값 덮어쓰게 됨)
                    if (fileTask.src.dirFd != -1) {
                        close(fileTask.src.dirFd);
                        fileTask.src.dirFd = -1;
                    }
                    fileTask.type = (ch == 'c' || ch == 'C') ? COPY : MOVE;  // 복사/삭제 결정
                    fileTask.src = getCurrentSelectedItem();  // 현재 선택된 Item 정보 가져옴
                    // 현재 폴더의 fd 가져옴
                    curWin = getCurrentWindow();
                    pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
                    fileTask.src.dirFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
                    pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
                    break;
                case 'v':
                case 'V':  // 붙여넣기: 미리 복사/잘라내기 된 파일 있으면 수행, 없으면 오류 표시
                    if (fileTask.src.dirFd == -1) {  // 선택된 파일 없음
                        // TODO: 오류 표시
                        break;
                    }
                    fileTask.dst = getCurrentSelectedItem();  // 현재 선택된 Item 정보 가져옴
                    strcpy(fileTask.dst.name, fileTask.src.name);  // 목적지 이름 설정 (Rename Operation 대비)
                    // 현재 폴더의 fd 가져옴
                    curWin = getCurrentWindow();
                    pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
                    fileTask.dst.dirFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
                    pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
                    // pipe에 명령 쓰기
                    write(pipeFileOpCmd, &fileTask, sizeof(FileTask));  // 구조체 크기 < PIPE_BUF(=4096) -> Atomic, 별도 보호 불필요
                    fileTask.src.dirFd = -1;  // '덮어쓰기'될 fd 아님: 다음 Copy/Move 대상 지정 시, close 방지
                    break;
                case KEY_DC:  // Delete 키
                    // fileTask.type = DELETE;  // '삭제' 전용 변수: 종류 대입 불필요
                    fileDelTask.src = getCurrentSelectedItem();  // 현재 선택된 Item 정보 가져옴
                    // 현재 폴더의 fd 가져옴
                    curWin = getCurrentWindow();
                    pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
                    fileDelTask.src.dirFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
                    pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
                    // pipe에 명령 쓰기
                    write(pipeFileOpCmd, &fileDelTask, sizeof(FileTask));  // 구조체 크기 < PIPE_BUF(=4096) -> Atomic, 별도 보호 불필요
                    break;

                // 종료
                case 'q':
                case 'Q':
                    goto CLEANUP;  // Main Loop 빠져나감

                default:
                    mvwhline(stdscr, getmaxy(stdscr) - 3, 0, ACS_HLINE, getmaxy(stdscr));
                    mvwprintw(stdscr, getmaxy(stdscr) - 3, 2, "0x%x 0x%lx 0x%lx", ch, ch & ~BUTTON_CTRL, ~BUTTON_CTRL);
                    break;
            }
        }

        // 제목 영역 업데이트
        renderTime();  // 시간 업데이트

        // 현재 창의 Working Directory 가져옴
        curWin = getCurrentWindow();
        pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
        cwdFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
        pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
        if (snprintf(fdPathBuf, MAX_PATH_LEN, "/proc/self/fd/%d", cwdFd) == MAX_PATH_LEN) {
            cwdLen = -1;
        } else {
            cwdLen = readlink(fdPathBuf, cwdBuf, MAX_PATH_LEN);
        }
        close(cwdFd);
        if (cwdLen == -1) {
            printPath("-----", 5);
        } else {
            cwdBuf[MAX_PATH_LEN - 1] = '\0';
            printPath(cwdBuf, (size_t)cwdLen);
        }

        updateDirWins();  // 폴더 표시 창들 업데이트

        displayProgress(fileProgresses);

        // pthread_mutex_lock(&processWindow.visibleMutex);
        // if (processWindow.isWindowVisible) {
        //     pthread_mutex_unlock(&processWindow.visibleMutex);  // Unlock before update
        //     updateProcessWindow(&processWindow);
        // } else {
        //     pthread_mutex_unlock(&processWindow.visibleMutex);  // Unlock if not visible
        // }
        if (showProcessWindow) {
            if (!prevShowProcessWindow) {
                pthread_mutex_lock(&processThreadArgs.commonArgs.statusMutex);
                processThreadArgs.commonArgs.statusFlags &= ~LISTPROCESS_FLAG_PAUSE_THREAD;
                pthread_cond_signal(&processThreadArgs.commonArgs.resumeThread);
                pthread_mutex_unlock(&processThreadArgs.commonArgs.statusMutex);
            }
            updateProcessWindow();
            prevShowProcessWindow = true;
        } else if (prevShowProcessWindow) {
            hideProcessWindow();
            pthread_mutex_lock(&processThreadArgs.commonArgs.statusMutex);
            processThreadArgs.commonArgs.statusFlags |= LISTPROCESS_FLAG_PAUSE_THREAD;
            pthread_mutex_unlock(&processThreadArgs.commonArgs.statusMutex);
            prevShowProcessWindow = false;
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
#pragma GCC diagnostic pop

void stopThreads(void) {
    // Thread들 정지 요청
    for (int i = 0; i < dirWinCnt && i < MAX_DIRWINS; i++) {
        pthread_mutex_lock(&dirListenerArgs[i].commonArgs.statusMutex);
        dirListenerArgs[i].commonArgs.statusFlags |= THREAD_FLAG_STOP;
        dirListenerArgs[i].commonArgs.statusFlags &= ~SORT_CRITERION_MASK;  // 정렬 기준 플래그 초기화
        dirListenerArgs[i].commonArgs.statusFlags &= ~SORT_DIRECTION_BIT;  // 정렬 방향 플래그 초기화
        pthread_cond_signal(&dirListenerArgs[i].commonArgs.resumeThread);
        pthread_mutex_unlock(&dirListenerArgs[i].commonArgs.statusMutex);
    }
    pthread_mutex_lock(&processThreadArgs.commonArgs.statusMutex);
    processThreadArgs.commonArgs.statusFlags |= THREAD_FLAG_STOP;
    pthread_cond_signal(&processThreadArgs.commonArgs.resumeThread);
    pthread_mutex_unlock(&processThreadArgs.commonArgs.statusMutex);
    // Linux에서: pthread_mutex_destroy, pthread_cond_destroy 반드시 필요한 것 아님 (manpage 참조)
    close(pipeFileOpCmd);  // File Operator Thread용 pipe의 write end: close() -> Thread들 순차적으로 정지됨

    // 각 Thread들 대기
    for (int i = 0; i < dirWinCnt && i < MAX_DIRWINS; i++) {
        pthread_mutex_lock(&dirListenerArgs[i].commonArgs.statusMutex);
        if (dirListenerArgs[i].commonArgs.statusFlags & THREAD_FLAG_RUNNING) {
            pthread_mutex_unlock(&dirListenerArgs[i].commonArgs.statusMutex);
            pthread_join(threadListDir[i], NULL);
        } else {
            pthread_mutex_unlock(&dirListenerArgs[i].commonArgs.statusMutex);
        }
    }
    for (int i = 0; i < dirWinCnt && i < MAX_FILE_OPERATORS; i++) {
        pthread_mutex_lock(&fileOpArgs[i].commonArgs.statusMutex);
        if (fileOpArgs[i].commonArgs.statusFlags & THREAD_FLAG_RUNNING) {
            pthread_mutex_unlock(&fileOpArgs[i].commonArgs.statusMutex);
            pthread_join(threadFileOperators[i], NULL);
        } else {
            pthread_mutex_unlock(&fileOpArgs[i].commonArgs.statusMutex);
        }
    }
    pthread_mutex_lock(&processThreadArgs.commonArgs.statusMutex);
    if (processThreadArgs.commonArgs.statusFlags & THREAD_FLAG_RUNNING) {
        pthread_mutex_unlock(&processThreadArgs.commonArgs.statusMutex);
        pthread_join(threadProcess, NULL);
    } else {
        pthread_mutex_unlock(&processThreadArgs.commonArgs.statusMutex);
    }
}

void cleanup(void) {
    endwin();
}
