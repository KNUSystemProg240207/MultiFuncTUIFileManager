#include <assert.h>
#include <curses.h>
#include <limits.h>
#include <panel.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "colors.h"
#include "commons.h"
#include "config.h"
#include "dir_entry_utils.h"
#include "dir_listener.h"
#include "dir_window.h"
#include "file_operator.h"


/**
 * @struct _DirWin
 * Directory Window의 정보 저장
 *
 * @var _DirWin::win WINDOW 구조체
 * @var _DirWin::order Directory 창 순서 (가장 왼쪽=0) ( [0, MAX_DIRWINS) )
 * @var _DirWin::currentPos 현재 선택된 Element
 * @var _DirWin::bufMutex dirEntry 보호 Mutex
 * @var _DirWin::dirEntry 폴더 항목들
 * @var _DirWin::totalReadItems 현 폴더에서 읽어들인 항목 수
 *   일반적으로, 디렉토리에 있는 파일, 폴더의 수와 같음
 *   단, buffer 공간 부족한 경우, 최대 buffer 길이
 * @var _DirWin::lineMovementEvent 창별 줄 이동 Event 저장 (bit field)
 * @var _DirWin::sortFlag 정렬 관련 Flag들
 */
struct _DirWin {
    WINDOW *win;  // WINDOW 구조체
    unsigned int order;  // 창 순서 (가장 왼쪽=0) ( [0, MAX_DIRWINS) )
    size_t currentPos;  // 현재 선택된 Element
    pthread_mutex_t *bufMutex;  // dirEntry 보호 Mutex
    DirEntry *dirEntry;  // 폴더 항목들
    size_t *totalReadItems;  // 현 폴더에서 읽어들인 항목 수
    uint64_t lineMovementEvent;  // 창별 줄 이동 Event 저장 (bit field)
    uint8_t sortFlag;  // 정렬 관련 Flag들
};
typedef struct _DirWin DirWin;


static DirWin windows[MAX_DIRWINS];  // 각 창의 runtime 정보 저장
static PANEL *panels[MAX_DIRWINS];  // 패널 배열
static int winCnt;  // 창 개수
static int showingWinCnt;  // '보여지는' 창 개수
static int currentWin;  // 현재 창의 Index
static bool changeWinSize = false;  // 창 크기 변경 필요

/**
 * 창 위치 계산
 *
 * @param y (반환) 창 좌상단 y좌표
 * @param x (반환) 창 좌상단 x좌표
 * @param h (반환) 창 높이
 * @param w (반환) 창 폭
 * @param winNo 창의 순서 (가장 왼쪽=0) ( [0, winCnt) )
 * @param winCnt 총 창의 개수
 * @return 성공: 0, 부적절한 인자 전달한 경우: -1
 */
static int calculateWinPos(int *y, int *x, int *h, int *w, unsigned int winNo, unsigned int winCnt);

/**
 * 디렉토리 창의 파일 목록 상단 헤더 출력
 *
 * @param win 디렉토리 표시 창
 * @param winH 창의 높이
 * @param winW 창의 너비
 */
static void printFileHeader(DirWin *win, int winH, int winW);

/**
 * 디렉토리 항목 정보 출력
 *
 * @param win 디렉토리 표시 창
 * @param startIdx 출력 시작 인덱스
 * @param line 출력할 줄 번호
 * @param winW 창의 너비
 */
static void printFileInfo(DirWin *win, int startIdx, int line, int winW);


int initDirWin(
    pthread_mutex_t *bufMutex,
    size_t *totalReadItems,
    DirEntry *dirEntry
) {
    if (winCnt >= MAX_DIRWINS) {
        // 최대 창 개수 초과
        return -1;
    }

    int y, x, h, w;
    // 위치 계산
    if (calculateWinPos(&y, &x, &h, &w, winCnt, winCnt + 1) == -1) {
        return -1;
    }
    // 창 생성
    WINDOW *newWin = newwin(h, w, y, x);
    if (newWin == NULL) {
        return -1;
    }

    // Panel 초기화
    PANEL *newPanel = new_panel(newWin);
    if (newPanel == NULL) {
        delwin(newWin);
        return -1;
    }
    panels[winCnt] = newPanel;

    // 변수 설정
    windows[winCnt] = (DirWin) {
        .win = newWin,
        .order = winCnt,
        .currentPos = 0,
        .bufMutex = bufMutex,
        .totalReadItems = totalReadItems,
        .sortFlag = 0x01,  // 기본 정렬 방식은 이름 오름차순
        .dirEntry = dirEntry
    };
    return winCnt++;
}

int updateDirWins(void) {
    static int prevScreenH = 0, prevScreenW = 0;

    int winY, winX, winH, winW;
    int screenH, screenW;
    int availableH;
    int ret;
    int lineMovement;
    int centerLine, currentLine;
    int itemsToPrint;
    ssize_t itemsCnt;
    size_t startIdx;
    DirWin *win;

    getmaxyx(stdscr, screenH, screenW);
    if (prevScreenH != screenH || prevScreenW != screenW) {
        prevScreenH = screenH;
        prevScreenW = screenW;
        changeWinSize = true;
    }

    // 각 창들 업데이트
    int eventStartPos;  // 이벤트 시작 위치
    int i;  // 내부 출력 for 문에서 사용할 변수
    for (int winNo = 0; winNo < showingWinCnt; winNo++) {
        win = windows + winNo;

        if (changeWinSize) {
            // 창 크기 변경 필요하면: 바꾸기
            if (calculateWinPos(&winY, &winX, &winH, &winW, winNo, showingWinCnt) == -1) {
                return -1;
            }
            wresize(win->win, winH, winW);
            replace_panel(panels[winNo], win->win);
            move_panel(panels[winNo], winY, winX);
        } else {
            // 창 크기 가져오기
            getmaxyx(win->win, winH, winW);
        }

        // Mutex 획득 시도: 실패 시, 파일 정보 업데이트 중: 해당 창 업데이트 건너뜀
        ret = pthread_mutex_trylock(win->bufMutex);
        if (ret != 0)
            continue;
        itemsCnt = *win->totalReadItems;  // 읽어들인 개수 가져옴

        // 현재 선택이 범위 벗어난 경우 (파일 삭제 등으로 인한) -> 범위 안으로 보내기
        if (win->currentPos >= itemsCnt - 1)
            win->currentPos = itemsCnt - 1;

        // 첫 Event의 위치 찾아내기: loop 끝나면, eventStartPos는 (event 수 * 2)에 해당하는 값 가짐 (매 event의 크기: 2-bit)
        for (eventStartPos = 0; (win->lineMovementEvent & (UINT64_C(2) << eventStartPos)) != 0; eventStartPos += 2);

        // 처음 event부터 선택 항목 이동 처리
        while (eventStartPos > 0) {  // 남은 event 있는 동안
            eventStartPos -= 2;  // 다음 event로 이동
            lineMovement = (win->lineMovementEvent >> eventStartPos) & 0x03;  // 현 위치 event만 가져옴
            switch (lineMovement) {
                case 0x02:  // '한 칸 위로 이동' Event
                    if (win->currentPos > 0)  // 위로 이동 가능하면
                        win->currentPos--;
                    else
                        win->currentPos = 0;  // Corner case 처리: 최소 Index
                    break;
                case 0x03:  // '한 칸 아래로 이동' Event
                    if (win->currentPos < itemsCnt - 1)  // 아래로 이동 가능하면
                        win->currentPos++;
                    else
                        win->currentPos = itemsCnt - 1;  // Corner case 처리: 최대 Index
                    break;
            }
        }
        win->lineMovementEvent = 0;  // Event 모두 삭제

        availableH = winH - 4;  // 최대 출력 가능한 라인 넘버 -4

        if (isColorSafe)
            wbkgd(win->win, COLOR_PAIR(BGRND));  // 창 색깔 변경
        printFileHeader(win, winH, winW);  // 최상단 Header 출력

        // 라인 스크롤
        centerLine = (availableH - 1) / 2;  // 가운데 줄의 줄 번호 ( [0, availableH) )
        if (itemsCnt <= availableH) {  // 항목 개수 적음 -> 빠르게 처리
            startIdx = 0;
            itemsToPrint = itemsCnt;
        } else if (win->currentPos < centerLine) {  // 위쪽 item 선택됨
            startIdx = 0;
            itemsToPrint = itemsCnt < availableH ? itemsCnt : availableH;
        } else if (win->currentPos >= itemsCnt - (availableH - centerLine - 1)) {  // 아래쪽 item 선택된 경우 (우변: 개수 - 커서 아래쪽에 출력될 item 수)
            startIdx = itemsCnt - availableH;
            itemsToPrint = availableH;
        } else {  // 일반적인 경우
            startIdx = win->currentPos - centerLine;
            itemsToPrint = availableH;
        }

        currentLine = win->currentPos - startIdx;  // 역상으로 출력할, 현재 선택된 줄

        // 디렉토리 출력
        for (i = 0; i < itemsToPrint; i++) {  // 항목 있는 공간: 출력
            if (winNo == currentWin && i == currentLine)  // 선택된 것: 역상으로 출력
                wattron(win->win, A_REVERSE);
            printFileInfo(win, startIdx, i, winW);
            if (winNo == currentWin && i == currentLine)
                wattroff(win->win, A_REVERSE);
        }
        wmove(win->win, i + 3, 0);  // 커서 위치 이동, 이걸 넣어야 맨 아랫줄 공백을 wclrtobot로 안 지움
        wclrtobot(win->win);  // 커서 아래 남는 공간: 지움
        box(win->win, 0, 0);
        pthread_mutex_unlock(win->bufMutex);
    }
    changeWinSize = false;

    return 0;
}


// 윈도우 헤더 출력 함수
void printFileHeader(DirWin *win, int winH, int winW) {
    // 색깔 적용
    applyColor(win->win, HEADER);

    // 정렬 상태에 따른 헤더 출력 준비
    char nameHeader[30] = "    FILE NAME  ";
    char sizeHeader[10] = "  SIZE  ";
    char dateHeader[20] = "  LAST MODIFIED  ";

    // 이름 정렬 상태 화살표
    if (((win->sortFlag & SORT_NAME_MASK) >> SORT_NAME_SHIFT) == 1) {
        strcat(nameHeader, "v");
    } else if (((win->sortFlag & SORT_NAME_MASK) >> SORT_NAME_SHIFT) == 2) {
        strcat(nameHeader, "^");
    }

    // 크기 정렬 상태 화살표
    if (((win->sortFlag & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT) == 1) {
        strcat(sizeHeader, "v");
    } else if (((win->sortFlag & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT) == 2) {
        strcat(sizeHeader, "^");
    }

    // 날짜 정렬 상태 화살표
    if (((win->sortFlag & SORT_DATE_MASK) >> SORT_DATE_SHIFT) == 1) {
        strcat(dateHeader, "v");
    } else if (((win->sortFlag & SORT_DATE_MASK) >> SORT_DATE_SHIFT) == 2) {
        strcat(dateHeader, "^");
    }

    // 헤더 출력 파트
    if (winW >= 54) {
        // 최대 너비
        mvwprintw(win->win, 1, 1, "%-20s|%-10s|%-19s|", nameHeader, sizeHeader, dateHeader);
    } else if (winW >= 40) {
        // 중간 너비
        mvwprintw(win->win, 1, 1, "%-20s|%-19s|", nameHeader, dateHeader);
    } else if (winW >= 35) {
        mvwprintw(win->win, 1, 1, "%-20s|%-12s|", nameHeader, sizeHeader);
    } else {
        // 최소 너비
        mvwprintw(win->win, 1, 1, "%-20s|", nameHeader);
    }

    whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 채우기

    // 색깔 해제
    removeColor(win->win, HEADER);
    mvwhline(win->win, 2, 1, 0, winW - 2);  // 구분선 출력
}

// 파일 목록 출력 함수
void printFileInfo(DirWin *win, int startIdx, int line, int winW) {
    struct stat *fileStat = &(win->dirEntry[startIdx + line].statEntry);  // 파일 스테이터스
    char *fileName = win->dirEntry[startIdx + line].entryName;  // 파일 이름
    size_t fileSize = fileStat->st_size;  // 파일 사이즈
    char lastModDate[20];  // 날짜가 담기는 문자열
    char lastModTime[20];  // 시간이 담기는 문자열
    int displayLine = line + 3;  // 출력되는 실제 라인 넘버
    const char *format;  // 출력 포맷

    // 마지막 수정 시간
    struct tm tm;
    localtime_r(&fileStat->st_mtime, &tm);
    strftime(lastModDate, sizeof(lastModDate), "%y/%m/%d", &tm);
    strftime(lastModTime, sizeof(lastModTime), "%H:%M", &tm);

    // 색상 선택
    int colorPair = DEFAULT;
    if ((S_ISDIR(fileStat->st_mode)) && isHidden(fileName)) {
        colorPair = HIDDEN_FOLDER;
    } else if (S_ISDIR(fileStat->st_mode)) {
        colorPair = DIRECTORY;
    } else if (isHidden(fileName)) {
        colorPair = HIDDEN;
    } else if (S_ISLNK(fileStat->st_mode)) {
        colorPair = SYMBOLIC;
    } else if (isImageFile(fileName)) {
        colorPair = IMG;
    } else if (isEXE(fileName)) {
        colorPair = EXE;
    }
    // 파일 이름 너무 길면 자르기
    fileName = truncateFileName(fileName);


    // 색깔 적용
    applyColor(win->win, colorPair);

    // 출력 파트
    if (winW >= 54) {  // 최대 너비
        format = "%-20s %10s %13s %s";
        mvwprintw(win->win, displayLine, 1, format, fileName, formatSize(fileSize), lastModDate, lastModTime);
    } else if (winW >= 41) {  // 중간 너비
        format = "%-20s %13s %s";
        mvwprintw(win->win, displayLine, 1, format, fileName, lastModDate, lastModTime);
    } else if (winW >= 35) {  // 최소 너비
        format = "%-20s %12s";
        mvwprintw(win->win, displayLine, 1, format, fileName, formatSize(fileSize));
    } else {
        format = "%-20s";
        mvwprintw(win->win, displayLine, 1, format, fileName);
    }
    whline(win->win, ' ', getmaxx(win->win) - getcurx(win->win) - 1);  // 남은 공간 공백 처리 (박스용 -1)

    // 색깔 해제
    removeColor(win->win, colorPair);
}

// 정렬 상태 토글 함수
void toggleSort(int mask, int shift) {
#define SORT_FLAG (windows[currentWin].sortFlag)

    // 현재 상태를 추출
    int state = (SORT_FLAG & mask) >> shift;

    // 만약, 현재 상태가 00이면 이전의 다른 비트들을 모두 00으로 초기화
    if (state == 0) {
        // 다른 비트들은 모두 00으로 초기화
        SORT_FLAG &= ~SORT_NAME_MASK;  // 이름 기준 비트 초기화
        SORT_FLAG &= ~SORT_SIZE_MASK;  // 사이즈 기준 비트 초기화
        SORT_FLAG &= ~SORT_DATE_MASK;  // 날짜 기준 비트 초기화
    }

    // 상태 순환: 01 -> 10 -> 01 순으로 순환
    if (state == 0) {
        state = 1;  // 첫 번째 -> 두 번째 (오름차순)
    } else if (state == 1) {
        state = 2;  // 두 번째 -> 세 번째 (내림차순)
    } else {
        state = 1;  // 세 번째 -> 첫 번째 (오름차순)
    }

    // 해당 비트에 상태 반영
    SORT_FLAG = (SORT_FLAG & ~mask) | (state << shift);
}

int calculateWinPos(int *y, int *x, int *h, int *w, unsigned int winNo, unsigned int winCnt) {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);

    // 창의 개수에 따라 분기
    switch (winCnt) {
        case 1:  // 창 1개 존재
            if (winNo != 0)
                return -1;
            *y = 2;
            *x = 0;
            *h = screenH - 5;  // 상단 제목 창과 하단 단축키 창 제외
            *w = screenW;
            return 0;
        case 2:
            if (winNo != 0 && winNo != 1)
                return -1;
            *y = 2;
            *x = winNo * (screenW / 2 + screenW % 2);
            *h = screenH - 5;
            *w = screenW / 2 + screenW % 2 * (1 - winNo);
            return 0;
        case 3:
            if (winNo < 0 || winNo > 2)
                return -1;
            *y = 2;
            *x = winNo * (screenW / 3) + (screenW % 3 > 2 - winNo);
            *h = screenH - 5;
            if (winNo == 1) {
                *w = screenW / 3 + (screenW % 3 == 1);
            } else {
                *w = screenW / 3 + (screenW % 3 == 2);
            }
            return 0;
        default:
            return -1;
    }
}

/*
currentPos 변수 자체는 다른 thread에서 (추가로, 다른 file에서도) 접근 안 함 -> 별도 보호 없이 값 써도 안전
단, totalReadItems 변수는 다른 thread와 공유되는 자원 -> mutex 획득 필요
또한, moveCursorDown 함수는 현재 총 항목 수 필요
=> mutex 획득 없이 처리 위해, 별도로 event 저장 -> 나중에 mutex 획득 후, 한 번에 계산
*/

void moveCursorUp(void) {
    // 이미 공간 다 썼다면 (= MSB가 1) -> 이후 수신된 이벤트 버림
    if (windows[currentWin].lineMovementEvent & (UINT64_C(0x80) << (7 * 8)))
        return;
    windows[currentWin].lineMovementEvent = windows[currentWin].lineMovementEvent << 2 | 0x02;  // <한 칸 위로> Event 저장
}

void moveCursorDown(void) {
    // 이미 공간 다 썼다면 (= MSB가 1) -> 이후 수신된 이벤트 버림
    if (windows[currentWin].lineMovementEvent & (UINT64_C(0x80) << (7 * 8)))
        return;
    windows[currentWin].lineMovementEvent = windows[currentWin].lineMovementEvent << 2 | 0x03;  // <한 칸 아래로> Event 저장
}

void selectPreviousWindow(void) {
    if (currentWin == 0)
        currentWin = showingWinCnt - 1;
    else
        currentWin--;
}

void selectNextWindow(void) {
    if (currentWin == showingWinCnt - 1)
        currentWin = 0;
    else
        currentWin++;
}

int setDirWinCnt(int count) {
    if (count < 1 || count > winCnt)
        return -1;
    if (count != showingWinCnt) {
        changeWinSize = true;
        if (showingWinCnt > count) {
            for (int i = showingWinCnt - 1; i >= count; i--) {
                hide_panel(panels[i]);
            }
            if (currentWin >= count)
                currentWin = count - 1;
        } else {  // showingWinCnt < count
            for (int i = showingWinCnt; i < count; i++) {
                show_panel(panels[i]);
            }
        }
        showingWinCnt = count;
    }
    return 0;
}

SrcDstInfo getCurrentSelectedItem(void) {
    DirWin *currentWinArgs = windows + currentWin;
    size_t currentSelection = currentWinArgs->currentPos;
    assert(pthread_mutex_lock(currentWinArgs->bufMutex) == 0);
    struct stat *statEntry = &currentWinArgs->dirEntry[currentSelection].statEntry;
    SrcDstInfo result = {
        .dirFd = -1,  // Directory is unknown -> Prevent bug
        .mode = statEntry->st_mode,
        .devNo = statEntry->st_dev,
        .fileSize = statEntry->st_size
    };
    strcpy(result.name, currentWinArgs->dirEntry[currentSelection].entryName);
    pthread_mutex_unlock(currentWinArgs->bufMutex);
    return result;
}

void setCurrentSelection(size_t index) {
    windows[currentWin].currentPos = index;
}

unsigned int getCurrentWindow(void) {
    return currentWin;
}
