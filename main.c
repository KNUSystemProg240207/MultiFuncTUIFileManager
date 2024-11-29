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
#include "popup_window.h"
#include "process_window.h"
#include "title_bar.h"


#define CTRL_KEY(key) (key & 037)


typedef enum _ProgramState {
    NORMAL,
    PROCESS_WIN,
    RENAME_POPUP,
    CHDIR_POPUP,
    MKDIR_POPUP
} ProgramState;


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

static ProgramState state;
static int pipeFileOpCmd;  // File operator thread로 명령 전달 위한 pipe의 write end
static unsigned int dirWinCnt;  // 표시된 폴더 표시 창 수

static void initVariables(void);  // 변수들 초기화
static void initScreen(void);  // ncurses 관련 초기화 & subwindow들 생성
static void initThreads(void);  // thread 관련 초기화
static void mainLoop(void);  // 프로그램 Main Loop
static void stopThreads(void);  // 실행 중인 Thread들 정지
static void cleanup(void);  // atexit()에 전달할 함수: 프로그램 종료 직전, main() 반환 직후에 수행됨


static inline void ctrlCHandler(int signal) {
    ungetch(CTRL_KEY('c'));
}

int main(int argc, char **argv) {
    struct sigaction ctrlCSignal = {
        .sa_handler = ctrlCHandler,
        .sa_flags = SA_RESTART
    };
    sigfillset(&ctrlCSignal.sa_mask);
    sigaction(SIGINT, &ctrlCSignal, NULL);

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
    initBottomBox(w, h - 3);  // 아래쪽 단축키 창 생성
    initPopupWindow();
    CHECK_CURSES(mvhline(1, 0, ACS_HLINE, w));  // 제목 창 아래로 가로줄 그림
    CHECK_CURSES(mvhline(h - 3, 0, ACS_HLINE, w));  // 단축키 창 위로 가로줄 그림

    // 폴더 내용 표시 창 생성
    for (int i = 0; i < MAX_DIRWINS; i++) {
        initDirWin(
            &dirListenerArgs[i].bufMutex,
            &dirListenerArgs[i].totalReadItems,
            dirListenerArgs[i].dirEntries
        );
    }
    setDirWinCnt(1);
    dirWinCnt = 1;

    initProcessWindow(
        &processThreadArgs.entriesMutex,
        &processThreadArgs.totalReadItems,
        processThreadArgs.processEntries
    );
}

void initThreads(void) {
    // Directory Listener Thread 초기화, 실행
    DIR *currentDir;
    for (int i = 0; i < MAX_DIRWINS; i++) {
        assert((currentDir = opendir(".")) != NULL);
        dirListenerArgs[i].currentDir = currentDir;
        if (i != 0) {
            dirListenerArgs[i].commonArgs.statusFlags |= THREAD_FLAG_PAUSE;  // 첫 창과 이어진 Thread 제외하고 일시정지시킴
        } else {
            assert((directoryOpenArgs = fcntl(dirfd(currentDir), F_GETFL)) != -1);
        }
        startDirListender(&threadListDir[i], &dirListenerArgs[i]);
    }

    // 프로세스 스레드 시작
    processThreadArgs.commonArgs.statusFlags |= THREAD_FLAG_PAUSE;  // 초기: 일시정지 된 상태로 시작
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

/**
 * 일반적인 상태 (디렉터리 창 표시) 키 입력 처리
 * 자주 호출되는 함수 -> inline 함수로 선언
 */
static inline int normalKeyInput(int ch) {
    unsigned int curWin;  // 현재 창
    ssize_t currentSelection;  // 선택된 Item

    // File Task (copy/move 및 source 정보 등 저장)
    static FileTask fileTask = {
        .src.dirFd = -1,
    };
    // File Task (Delete 전용: Copy/Move 원본 지정에 영향 미치지 않게)
    static FileTask fileDelTask = {
        .type = DELETE,
    };

    char tmp[256];

    int cwdFd;  // 현재 선택된 창의 Working Directory File Descriptor
    DIR *newCwd;

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
            selectPreviousWindow();
            break;
        case KEY_RIGHT:
            selectNextWindow();
            break;

        // 디렉터리 변경
        case '\n':
        case KEY_ENTER:
            currentSelection = getCurrentSelectedDirectory();
            if (currentSelection >= 0) {
                curWin = getCurrentWindow();
                pthread_mutex_lock(&dirListenerArgs[curWin].bufMutex);
                strncpy(dirListenerArgs[curWin].newCwdPath, dirListenerArgs[curWin].dirEntries[currentSelection].entryName, PATH_MAX);
                dirListenerArgs[curWin].commonArgs.statusFlags |= DIRLISTENER_FLAG_CHANGE_DIR;
                pthread_mutex_unlock(&dirListenerArgs[curWin].bufMutex);
                pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
                pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                setCurrentSelection(0);
            }
            break;

        // 이름 변경 창 토글
        case KEY_F(2):
            state = RENAME_POPUP;
            break;
        // 경로 변경 창 토글
        case 0x1f:  // CTRL_KEY('/')
            state = CHDIR_POPUP;
            break;
        // 폴더 생성 창 토글
        case CTRL_KEY('n'):
            state = MKDIR_POPUP;
            break;

        // 프로세스 창 토글
        case 'p':
            state = PROCESS_WIN;
            break;

        // 창 열기
        case CTRL_KEY('t'):
            if (dirWinCnt == MAX_DIRWINS) {
                displayBottomMsg("Already opened max window", FRAME_PER_SECOND);
                break;
            }
            // 새 창의 Working Directory 설정
            curWin = getCurrentWindow();
            pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
            cwdFd = dirfd(dirListenerArgs[curWin].currentDir);
            if (cwdFd != -1) {
                cwdFd = openat(cwdFd, ".", directoryOpenArgs);
            }
            pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
            if (cwdFd != -1) {
                newCwd = fdopendir(cwdFd);
                if (newCwd != NULL) {
                    pthread_mutex_lock(&dirListenerArgs[dirWinCnt].dirMutex);
                    if (dirListenerArgs[dirWinCnt].currentDir != NULL)
                        closedir(dirListenerArgs[dirWinCnt].currentDir);
                    dirListenerArgs[dirWinCnt].currentDir = newCwd;
                    pthread_mutex_unlock(&dirListenerArgs[dirWinCnt].dirMutex);
                }
            }
            resumeThread(&dirListenerArgs[dirWinCnt].commonArgs);  // 새 창과 이어진 Thread 시작
            dirWinCnt++;
            setDirWinCnt(dirWinCnt);  // 새 창 표시
            break;
        // 현재 창 닫기
        case CTRL_KEY('r'):
            if (dirWinCnt == 1) {
                displayBottomMsg("There is only one window", FRAME_PER_SECOND);
                break;
            }
            dirWinCnt--;
            setDirWinCnt(dirWinCnt);  // 창 감추기
            pauseThread(&dirListenerArgs[dirWinCnt].commonArgs);  // 기존 창과 이어진 Thread 정지
            break;

        // 정렬 변경
        case 'w':  // F1 키 (이름 기준 오름차순)
            toggleSort(SORT_NAME_MASK, SORT_NAME_SHIFT);
            curWin = getCurrentWindow();
            pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
            // 현재 기준이 이름순인지 확인
            if ((dirListenerArgs[curWin].commonArgs.statusFlags & DIRLISTENER_FLAG_SORT_CRITERION_MASK) == DIRLISTENER_FLAG_SORT_NAME) {
                // 기준이 같다면 방향 토글
                dirListenerArgs[curWin].commonArgs.statusFlags ^= DIRLISTENER_FLAG_SORT_REVERSE;
            } else {
                // 기준이 다르면 기준 초기화 및 오름차순 설정
                dirListenerArgs[curWin].commonArgs.statusFlags &= ~(DIRLISTENER_FLAG_SORT_CRITERION_MASK | DIRLISTENER_FLAG_SORT_REVERSE);
                dirListenerArgs[curWin].commonArgs.statusFlags |= DIRLISTENER_FLAG_SORT_NAME;
            }
            pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
            pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
            break;
        case 'e':  // F2 키 (크기 기준 오름차순)
            toggleSort(SORT_SIZE_MASK, SORT_SIZE_SHIFT);
            curWin = getCurrentWindow();
            pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
            // 현재 기준이 크기순인지 확인
            if ((dirListenerArgs[curWin].commonArgs.statusFlags & DIRLISTENER_FLAG_SORT_CRITERION_MASK) == DIRLISTENER_FLAG_SORT_SIZE) {
                // 기준이 같다면 방향 토글
                dirListenerArgs[curWin].commonArgs.statusFlags ^= DIRLISTENER_FLAG_SORT_REVERSE;
            } else {
                // 기준이 다르면 기준 초기화 및 오름차순 설정
                dirListenerArgs[curWin].commonArgs.statusFlags &= ~(DIRLISTENER_FLAG_SORT_CRITERION_MASK | DIRLISTENER_FLAG_SORT_REVERSE);
                dirListenerArgs[curWin].commonArgs.statusFlags |= DIRLISTENER_FLAG_SORT_SIZE;
            }
            pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
            pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
            break;
        case 'r':  // F3 키 (날짜 기준 오름차순)
            toggleSort(SORT_DATE_MASK, SORT_DATE_SHIFT);
            curWin = getCurrentWindow();
            pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
            // 현재 기준이 날짜순인지 확인
            if ((dirListenerArgs[curWin].commonArgs.statusFlags & DIRLISTENER_FLAG_SORT_CRITERION_MASK) == DIRLISTENER_FLAG_SORT_DATE) {
                // 기준이 같다면 방향 토글
                dirListenerArgs[curWin].commonArgs.statusFlags ^= DIRLISTENER_FLAG_SORT_REVERSE;
            } else {
                // 기준이 다르면 기준 초기화 및 오름차순 설정
                dirListenerArgs[curWin].commonArgs.statusFlags &= ~(DIRLISTENER_FLAG_SORT_CRITERION_MASK | DIRLISTENER_FLAG_SORT_REVERSE);
                dirListenerArgs[curWin].commonArgs.statusFlags |= DIRLISTENER_FLAG_SORT_DATE;
            }
            pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
            pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
            break;

        // 복사, 잘라내기, 붙여넣기
        case CTRL_KEY('c'):  // 복사
        case CTRL_KEY('x'):  // 잘라내기
            // 파일 정보 저장
            // 기존에 Source로 선택된 Item 있었으면: 해당 dirfd close (여기서 값 덮어쓰게 됨)
            if (fileTask.src.dirFd != -1) {
                close(fileTask.src.dirFd);
                fileTask.src.dirFd = -1;
            }
            fileTask.type = (ch == CTRL_KEY('c')) ? COPY : MOVE;  // 복사/삭제 결정
            fileTask.src = getCurrentSelectedItem();  // 현재 선택된 Item 정보 가져옴
            // 현재 폴더의 fd 가져옴
            curWin = getCurrentWindow();
            pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
            fileTask.src.dirFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
            pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
            displayBottomMsg((ch == CTRL_KEY('c')) ? "File copied" : "File cutted", FRAME_PER_SECOND);
            break;
        case CTRL_KEY('v'):  // 붙여넣기: 미리 복사/잘라내기 된 파일 있으면 수행, 없으면 오류 표시
            if (fileTask.src.dirFd == -1) {  // 선택된 파일 없음
                displayBottomMsg("No file selected", FRAME_PER_SECOND);
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
            displayBottomMsg("File action requested", FRAME_PER_SECOND);
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
            displayBottomMsg("File delete requested", FRAME_PER_SECOND);
            break;

        // 종료
        case 'q':
        case 'Q':
            return 1;  // Main Loop 빠져나감

        default:
            sprintf(tmp, "0x%x 0x%x", ch, CTRL_KEY(ch));
            displayBottomMsg(tmp, FRAME_PER_SECOND / 2);
            break;
    }
    return 0;
}

void mainLoop(void) {
    struct timespec startTime;  // Iteration 시작 시간
    uint64_t elapsedUSec;  // Iteration 걸린 시간

    int cwdFd;  // 현재 선택된 창의 Working Directory File Descriptor
    ssize_t cwdLen;
    // char *cwd;  // 현재 선택된 창의 Working Directory
    char fdPathBuf[PATH_MAX];  // Working Directory의 Buffer
    char pathBuf[PATH_MAX];  // 각종 경로 저장 Buffer
    FileTask fileTask = {};

    unsigned int curWin;  // 현재 창

    ProgramState prevState = state;

// 아래에서, dirFd 변수들에 대해 fd leak 경고 발생
// File Operator Thread에서 close() -> 여기서 close()하면, Thread쪽에서 오류 발생
// 따라서, 의도적으로 leak되게 함
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-fd-leak"
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &startTime);  // 시작 시간 가져옴

        // 키 입력 처리
        for (int ch = wgetch(stdscr); ch != ERR; ch = wgetch(stdscr)) {
            switch (state) {
                case NORMAL:
                    if (normalKeyInput(ch))
                        goto CLEANUP;
                    break;
                case PROCESS_WIN:
                    if (ch == 'q' || ch == 'Q')
                        goto CLEANUP;
                    if (ch == 'p' || ch == 'P' || ch == '\n' || ch == KEY_ENTER)
                        state = NORMAL;
                    break;
                case RENAME_POPUP:
                    if (' ' <= ch && ch <= '~') {
                        putCharToPopup(ch);
                    } else if (ch == KEY_BACKSPACE) {
                        popCharFromPopup();
                        // } else if (ch == KEY_LEFT) {
                        // } else if (ch == KEY_RIGHT) {
                    } else if (ch == '\n') {
                        // 팝업창에서 새 이름 가져오기
                        getStringFromPopup(pathBuf);
                        // 이름 변경 요청
                        fileTask.type = MOVE;
                        fileTask.src = getCurrentSelectedItem();  // 현재 선택된 Item 정보 가져옴
                        fileTask.dst = fileTask.src;  // 나머지 정보는 같음
                        strcpy(fileTask.dst.name, pathBuf);  // 새 이름으로 변경
                        // 현재 폴더의 fd 가져옴
                        curWin = getCurrentWindow();
                        pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
                        fileTask.src.dirFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
                        fileTask.dst.dirFd = fileTask.src.dirFd;
                        pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
                        // pipe에 명령 쓰기
                        write(pipeFileOpCmd, &fileTask, sizeof(FileTask));  // 구조체 크기 < PIPE_BUF(=4096) -> Atomic, 별도 보호 불필요
                        displayBottomMsg("Rename requested", FRAME_PER_SECOND);
                        // 창 닫기
                        state = NORMAL;
                    } else if (ch == KEY_F(2)) {
                        state = NORMAL;  // 창 닫기
                    }
                    break;
                case CHDIR_POPUP:
                    if (' ' <= ch && ch <= '~') {
                        putCharToPopup(ch);
                    } else if (ch == KEY_BACKSPACE) {
                        popCharFromPopup();
                        // } else if (ch == KEY_LEFT) {
                        // } else if (ch == KEY_RIGHT) {
                    } else if (ch == '\n') {
                        // Working directory 변경 수행
                        // 팝업창에서 경로 가져오기
                        curWin = getCurrentWindow();
                        pthread_mutex_lock(&dirListenerArgs[curWin].bufMutex);
                        getStringFromPopup(dirListenerArgs[curWin].newCwdPath);
                        dirListenerArgs[curWin].commonArgs.statusFlags |= DIRLISTENER_FLAG_CHANGE_DIR;
                        pthread_mutex_unlock(&dirListenerArgs[curWin].bufMutex);
                        pthread_mutex_lock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                        pthread_cond_signal(&dirListenerArgs[curWin].commonArgs.resumeThread);
                        pthread_mutex_unlock(&dirListenerArgs[curWin].commonArgs.statusMutex);
                        setCurrentSelection(0);
                        displayBottomMsg("Change working directory requested", FRAME_PER_SECOND);
                        // 창 닫기
                        state = NORMAL;
                    } else if (ch == 0x1f) {  // CTRL_KEY('/')
                        state = NORMAL;  // 창 닫기
                    }
                    break;
                case MKDIR_POPUP:
                    if (' ' <= ch && ch <= '~') {
                        putCharToPopup(ch);
                    } else if (ch == KEY_BACKSPACE) {
                        popCharFromPopup();
                        // } else if (ch == KEY_LEFT) {
                        // } else if (ch == KEY_RIGHT) {
                    } else if (ch == '\n') {
                        // 폴더 생성 요청
                        // 팝업창에서 새 이름 가져오기
                        getStringFromPopup(pathBuf);
                        // 폴더 생성 요청
                        fileTask.type = MKDIR;
                        strcpy(fileTask.src.name, pathBuf);  // 새 이름 설정
                        // 현재 폴더의 fd 가져옴
                        curWin = getCurrentWindow();
                        pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
                        fileTask.src.dirFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
                        pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
                        // pipe에 명령 쓰기
                        write(pipeFileOpCmd, &fileTask, sizeof(FileTask));  // 구조체 크기 < PIPE_BUF(=4096) -> Atomic, 별도 보호 불필요
                        displayBottomMsg("Rename requested", FRAME_PER_SECOND);
                        // 창 닫기
                        state = NORMAL;
                    } else if (ch == CTRL_KEY('n')) {
                        state = NORMAL;  // 창 닫기
                    }
                    break;
            }
        }

        // 현재 창의 Working Directory 가져옴
        curWin = getCurrentWindow();
        pthread_mutex_lock(&dirListenerArgs[curWin].dirMutex);
        cwdFd = dup(dirfd(dirListenerArgs[curWin].currentDir));
        pthread_mutex_unlock(&dirListenerArgs[curWin].dirMutex);
        if (snprintf(fdPathBuf, PATH_MAX, "/proc/self/fd/%d", cwdFd) == PATH_MAX) {
            cwdLen = -1;
        } else {
            cwdLen = readlink(fdPathBuf, pathBuf, PATH_MAX);
        }
        close(cwdFd);

        updateTitleBar(pathBuf, (size_t)cwdLen);
        updateDirWins();  // 폴더 표시 창들 업데이트
        updateBottomBox(fileProgresses);

        switch (state) {
            case PROCESS_WIN:
                if (prevState != PROCESS_WIN) {
                    resumeThread(&processThreadArgs.commonArgs);
                    prevState = PROCESS_WIN;
                }
                updateProcessWindow();
                break;
            case RENAME_POPUP:
                if (prevState != RENAME_POPUP) {
                    showPopupWindow("Rename");
                    prevState = RENAME_POPUP;
                }
                updatePopupWindow();
                break;
            case CHDIR_POPUP:
                if (prevState != CHDIR_POPUP) {
                    showPopupWindow("Enter new path");
                    prevState = CHDIR_POPUP;
                }
                updatePopupWindow();
                break;
            case MKDIR_POPUP:
                if (prevState != MKDIR_POPUP) {
                    showPopupWindow("Enter new directory name");
                    prevState = MKDIR_POPUP;
                }
                updatePopupWindow();
                break;
            default:
                if (prevState == PROCESS_WIN) {
                    hideProcessWindow();
                    pauseThread(&processThreadArgs.commonArgs);
                    prevState = NORMAL;
                } else if (prevState == RENAME_POPUP || prevState == CHDIR_POPUP || prevState == MKDIR_POPUP) {
                    hidePopupWindow();
                    prevState = NORMAL;
                }
                break;
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

static inline void tryJoinThread(ThreadArgs *commonArgs, pthread_t *threadToWait) {
    pthread_mutex_lock(&commonArgs->statusMutex);
    if (commonArgs->statusFlags & THREAD_FLAG_RUNNING) {
        pthread_mutex_unlock(&commonArgs->statusMutex);
        pthread_join(*threadToWait, NULL);
    } else {
        pthread_mutex_unlock(&commonArgs->statusMutex);
    }
}

void stopThreads(void) {
    // Thread들 정지 요청
    close(pipeFileOpCmd);  // File Operator Thread용 pipe의 write end: close() -> Thread들 순차적으로 정지됨
    for (int i = 0; i < MAX_DIRWINS; i++)
        stopThread(&dirListenerArgs[i].commonArgs);
    stopThread(&processThreadArgs.commonArgs);

    // 각 Thread들 대기
    for (int i = 0; i < MAX_DIRWINS; i++)
        tryJoinThread(&dirListenerArgs[i].commonArgs, &threadListDir[i]);
    for (int i = 0; i < MAX_FILE_OPERATORS; i++)
        tryJoinThread(&fileOpArgs[i].commonArgs, &threadFileOperators[i]);
    tryJoinThread(&processThreadArgs.commonArgs, &threadProcess);

    // Linux에서: pthread_mutex_destroy, pthread_cond_destroy 반드시 필요한 것 아님 (manpage 참조)
}

void cleanup(void) {
    endwin();
}
