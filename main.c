#include <curses.h>
#include <string.h>     // memset 함수용
#include <errno.h>      // errno 변수용
#include <libgen.h>     // basename 함수용
#include <fcntl.h>      // O_DIRECTORY 등 파일 옵션용
#include <unistd.h>     // 파일 작업용
#include <libgen.h>  // basename 함수용

#include "config.h"
#include "commons.h"
#include "dir_window.h"
#include "title_bar.h"
#include "bottom_area.h"
#include "file_ops.h"   // FileTask 구조체와 파일 작업 함수들
#include "file_manager.h"  // getPathInput 함수용


// MAX_PATH_LEN 정의 추가 
#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 4096
#endif

WINDOW *titleBar, *bottomBox;

// 각 (폴더의 내용 가져오는) Directory Listener Thread별 저장 공간:
static pthread_t threadListDir[MAX_DIRWINS];
static pthread_mutex_t statMutex[MAX_DIRWINS];
static struct stat statEntries[MAX_DIRWINS][MAX_STAT_ENTRIES];
static char entryNames[MAX_DIRWINS][MAX_STAT_ENTRIES][MAX_NAME_LEN + 1];
static pthread_cond_t condStopTrd[MAX_DIRWINS];
static bool stopRequested[MAX_DIRWINS];
static pthread_mutex_t stopTrdMutex[MAX_DIRWINS];
static size_t totalReadItems[MAX_DIRWINS] = { 0 };

// 파일 경로 관련 전역 변수 추가
static char src_path[MAX_CWD_LEN];
static char dst_path[MAX_CWD_LEN];
static char selected_path[MAX_CWD_LEN];

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
        pthread_mutex_lock(&stopTrdMutex[i]);
        stopRequested[i] = true;
        pthread_cond_signal(&condStopTrd[i]);
        pthread_mutex_unlock(&stopTrdMutex[i]);
    }
    // 각 Thread들 대기
    for (int i = 0; i < dirWinCnt; i++) {
        pthread_join(threadListDir[i], NULL);
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
        pthread_cond_init(condStopTrd + i, NULL);
        pthread_mutex_init(stopTrdMutex + i, NULL);
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
        &threadListDir[0], &statMutex[0],
        statEntries[0], entryNames[0], MAX_STAT_ENTRIES,
        &totalReadItems[0], &condStopTrd[0], &stopRequested[0], &stopTrdMutex[0]
    );  // Directory Listener Thread 시작
}

void mainLoop(void) {
    struct timespec startTime;
    uint64_t elapsedUSec;
    char cwdBuf[MAX_CWD_LEN];

    // 경로 버퍼 초기화
    memset(src_path, 0, MAX_CWD_LEN);
    memset(dst_path, 0, MAX_CWD_LEN);
    memset(selected_path, 0, MAX_CWD_LEN);

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
                case 'c':
                case 'C':
                    {
                        char src_path[MAX_CWD_LEN];
                        char dst_path[MAX_CWD_LEN];
                        const char *selected_file = getCurrentSelection();
                        
                        if (selected_file) {
                            snprintf(src_path, MAX_CWD_LEN, "%s/%s", cwdBuf, selected_file);
                            snprintf(dst_path, MAX_CWD_LEN, "%s/%s_copy", cwdBuf, selected_file);
                            FileTask task = {
                                .type = COPY,
                                .srcDirFd = open(".", O_DIRECTORY),
                                .srcDevNo = 0,
                                .fileSize = 0,
                                .dstDirFd = -1,
                                .dstDevNo = 0
                            };
                            strncpy(task.srcName, selected_file, MAX_NAME_LEN);
                            snprintf(task.dstName, MAX_NAME_LEN, "%s_copy", selected_file);
                            copyFileOperation(&task, 0, task.fileSize);
                            
                            if (task.srcDirFd >= 0) close(task.srcDirFd);
                            if (task.dstDirFd >= 0) close(task.dstDirFd);
                        }
                    }
                    break;

                case 'm':
                case 'M':
                    {
                        char src_path[MAX_CWD_LEN];
                        char dst_path[MAX_CWD_LEN];
                        const char *selected_file = getCurrentSelection();
                        
                        if (selected_file) {
                            snprintf(src_path, MAX_CWD_LEN, "%s/%s", cwdBuf, selected_file);
                            snprintf(dst_path, MAX_CWD_LEN, "%s/%s_moved", cwdBuf, selected_file);
                            FileTask task = {
                                .type = MOVE,
                                .srcDirFd = open(".", O_DIRECTORY),
                                .srcDevNo = 0,
                                .fileSize = 0,
                                .dstDirFd = -1,
                                .dstDevNo = 0
                            };
                            strncpy(task.srcName, selected_file, MAX_NAME_LEN);
                            snprintf(task.dstName, MAX_NAME_LEN, "%s_moved", selected_file);
                            moveFileOperation(&task);
                            
                            if (task.srcDirFd >= 0) close(task.srcDirFd);
                            if (task.dstDirFd >= 0) close(task.dstDirFd);
                        }
                    }
                    break;

                case 'd':
                case 'D':
                    {
                        const char *selected_file = getCurrentSelection();
                        if (selected_file) {
                            FileTask task = {
                                .type = DELETE,
                                .srcDirFd = open(cwdBuf, O_DIRECTORY),
                                .srcName = selected_file
                            };
                            deleteFileOperation(&task);
                            
                            if (task.srcDirFd >= 0) close(task.srcDirFd);
                        }
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
        char *cwd = getcwd(cwdBuf, MAX_CWD_LEN);  // 경로 가져옴
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

// FileTask 구조체 초기화 및 실행 예시
int executeFileOperation(FileOperation op) {
    FileTask task;
    memset(&task, 0, sizeof(FileTask));
    
    // 현재 선택된 파일 정보 가져오기
    const char* currentDir = getCurrentDirectory();  // 구현 필요
    const char* selectedFile = getCurrentSelection();  // 구현 필요
    
    // 기본 정보 설정
    task.type = op;
    task.srcDirFd = open(currentDir, O_DIRECTORY);
    strncpy(task.srcName, selectedFile, MAX_NAME_LEN);
    struct stat st;
    if (fstatat(task.srcDirFd, task.srcName, &st, 0) == 0) {
        task.srcDevNo = st.st_dev;
        task.fileSize = st.st_size;
    }

    switch(op) {
        case COPY:
        case MOVE:
            {
                char dstPath[MAX_NAME_LEN];
                if (getPathInput("Enter destination", dstPath, MAX_NAME_LEN) == 0) {
                    // 목적지 디렉토리 설정
                    char* dstDir = dirname(strdup(dstPath));  // 구현 필요
                    task.dstDirFd = open(dstDir, O_DIRECTORY);
                    strncpy(task.dstName, basename(dstPath), MAX_NAME_LEN);
                    
                    if (op == COPY) {
                        ssize_t result = copyFileOperation(&task, 0, task.fileSize);
                        if(result<0){
                            return -1;
                        }
                        // 결과 처리
                    } else {
                        int result = moveFileOperation(&task);
                        if (result<0){
                            return -1;
                        }
                        // 결과 처리
                    }
                }
            }
            break;
            
        case DELETE:
            {
                char confirm[2];
                if (getPathInput("Delete? (y/n)", confirm, 2) == 0 && 
                    (confirm[0] == 'y' || confirm[0] == 'Y')) {
                    int result = deleteFileOperation(&task);
                    // 결과 처리
                }
            }
            break;
    }

    // 정리
    if (task.srcDirFd >= 0) close(task.srcDirFd);
    if (task.dstDirFd >= 0) close(task.dstDirFd);

    return 0;
}

void showOperationResult(const char* operation, int result) {
    char message[100];
    if (result >= 0) {
        snprintf(message, sizeof(message), "%s completed successfully", operation);
    } else {
        snprintf(message, sizeof(message), "%s failed: %s", operation, strerror(errno));
    }
    // 메시지 표시 (구현 필요)
}