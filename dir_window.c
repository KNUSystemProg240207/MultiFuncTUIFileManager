#include <curses.h>
#include <limits.h>
#include <panel.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "colors.h"
#include "config.h"
#include "dir_window.h"

static DirWin windows[MAX_DIRWINS];  // 각 창의 runtime 정보 저장
static PANEL *panels[MAX_DIRWINS];  // 패널 배열
static unsigned int winCnt;  // 창 개수
static unsigned int currentWin;  // 현재 창의 Index
static int panelCnt = 0;  // 패널의 개수

void printFileHeader(DirWin *win, int winH, int winW);
void printFileInfo(DirWin *win, int startIdx, int line, int winW);


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

// 패널을 초기화하는 함수
int initPanelForWindow(WINDOW *win) {
    if (panelCnt >= MAX_DIRWINS) {
        return -1;  // 최대 창 개수 초과
    }

    // 창에 패널 추가
    panels[panelCnt] = new_panel(win);
    panelCnt++;

    box(win, 0, 0);

    // 패널 순서 업데이트 (이후 패널을 화면에 반영하려면 update_panels() 호출 필요)
    update_panels();
    doupdate();

    return 0;
}

// 윈도우에 패널 입히고 순서 조정
void refreshPanels() {
    for (int i = 0; i < panelCnt; i++) {
        // 각 패널의 상태를 새로 고침
        show_panel(panels[i]);
    }

    // 화면을 새로 고침하여 모든 패널을 업데이트
    update_panels();
    doupdate();
}

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

    // 패널 입히기
    initPanelForWindow(newWin);

    // 변수 설정
    windows[winCnt - 1] = (DirWin) {
        .win = newWin,
        .order = winCnt - 1,
        .currentPos = 0,
        .statMutex = statMutex,
        .statEntries = statEntries,
        .entryNames = entryNames,
        .totalReadItems = totalReadItems,
        .sortFlag = 0x01
    };
    return winCnt;
}

int updateDirWins(void) {
    int ret, winH, winW;
    int lineMovement;
    int centerLine, currentLine;
    int itemsToPrint;
    ssize_t itemsCnt;
    size_t startIdx;
    DirWin *win;

    // 각 창들 업데이트
    int eventStartPos;  // 이벤트 시작 위치
    int line;  // 내부 출력 for 문에서 사용할 변수
    for (int winNo = 0; winNo < winCnt; winNo++) {
        win = windows + winNo;

        // Mutex 획득 시도: 실패 시, 파일 정보 업데이트 중: 해당 창 업데이트 건너뜀
        ret = pthread_mutex_trylock(win->statMutex);
        if (ret != 0)
            continue;
        itemsCnt = *win->totalReadItems;  // 읽어들인 개수 가져옴

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
                    break;
                case 0x03:  // '한 칸 아래로 이동' Event
                    if (win->currentPos < itemsCnt - 1)  // 아래로 이동 가능하면
                        win->currentPos++;
                    break;
            }
        }
        win->lineMovementEvent = 0;  // Event 모두 삭제

        // 최상단에 출력될 항목의 index 계산
        getmaxyx(win->win, winH, winW);
        winH -= 4;  // 최대 출력 가능한 라인 넘버 -4
        // winW = 51;  // 윈도우 크기 확인용

        applySorting(win->sortFlag, win);  // 애초에 받아올 때, 정렬 안 된 상태로 받아오니까 계속 정렬을 해야 함

        wbkgd(win->win, COLOR_PAIR(BGRND));
        printFileHeader(win, winH, winW);  // 헤더 부분 출력

        /* 라인 스크롤 */
        centerLine = (winH - 1) / 2;  // 가운데 줄의 줄 번호 ( [0, winH) )
        if (itemsCnt <= winH) {  // 항목 개수 적음 -> 빠르게 처리
            startIdx = 0;
            itemsToPrint = itemsCnt;
        } else if (win->currentPos < centerLine) {  // 위쪽 item 선택됨
            startIdx = 0;
            itemsToPrint = itemsCnt < winH ? itemsCnt : winH;
        } else if (win->currentPos >= itemsCnt - (winH - centerLine - 1)) {  // 아래쪽 item 선택된 경우 (우변: 개수 - 커서 아래쪽에 출력될 item 수)
            startIdx = itemsCnt - winH;
            itemsToPrint = winH;
        } else {  // 일반적인 경우
            startIdx = win->currentPos - centerLine;
            itemsToPrint = winH;
        }
        refreshPanels();

        currentLine = win->currentPos - startIdx;  // 역상으로 출력할, 현재 선택된 줄

        /* 디렉토리 출력 */
        for (line = 0; line < itemsToPrint; line++) {  // 항목 있는 공간: 출력
            if (winNo == currentWin && line == currentLine)  // 선택된 것 역상으로 출력
                wattron(win->win, A_REVERSE);
            printFileInfo(win, startIdx, line, winW);
            if (winNo == currentWin && line == currentLine)
                wattroff(win->win, A_REVERSE);
        }
        // wclrtobot(win->win);  // 아래 남는 공간: 지움, 버그 나서 임시로 주석

        pthread_mutex_unlock(win->statMutex);
    }

    return 0;
}

void printFileHeader(DirWin *win, int winH, int winW) {
    wattron(win->win, COLOR_PAIR(HEADER));

    // 정렬 상태에 따른 헤더 출력 준비
    char nameHeader[30] = "    FILE NAME  ";
    char sizeHeader[10] = "  SIZE  ";
    char dateHeader[20] = "  LAST MODIFIED  ";

    // 이름 정렬 상태 추가
    if (((win->sortFlag & SORT_NAME_MASK) >> SORT_NAME_SHIFT) == 1) {
        strcat(nameHeader, "v");
    } else if (((win->sortFlag & SORT_NAME_MASK) >> SORT_NAME_SHIFT) == 2) {
        strcat(nameHeader, "^");
    }

    // 크기 정렬 상태 추가
    if (((win->sortFlag & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT) == 1) {
        strcat(sizeHeader, "v");
    } else if (((win->sortFlag & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT) == 2) {
        strcat(sizeHeader, "^");
    }

    // 날짜 정렬 상태 추가
    if (((win->sortFlag & SORT_DATE_MASK) >> SORT_DATE_SHIFT) == 1) {
        strcat(dateHeader, "v");
    } else if (((win->sortFlag & SORT_DATE_MASK) >> SORT_DATE_SHIFT) == 2) {
        strcat(dateHeader, "^");
    }

    // 헤더 출력
    if ((winW > 50) && (winW < 60)) {
        // 창이 2개일 때: 간략 헤더
        mvwprintw(win->win, 1, 1, "%-22s|%-20s|", nameHeader, dateHeader);
    } else {
        // 창이 1개일 때: 상세 헤더
        mvwprintw(win->win, 1, 1, "%-20s|%-10s|%-19s|", nameHeader, sizeHeader, dateHeader);
    }

    whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 현재 줄의 남은 공간: 공백으로 덮어씀 (역상 출력 위함)
    wattroff(win->win, COLOR_PAIR(HEADER));
    mvwhline(win->win, 2, 1, 0, winW - 2);  // 구분선
}


void printFileInfo(DirWin *win, int startIdx, int line, int winW) {
    struct stat *fileStat = win->statEntries + (startIdx + line);
    char *fileName = win->entryNames[startIdx + line];
    size_t fileSize = fileStat->st_size;
    char lastModDate[20];
    char lastModTime[20];

    // 파일 마지막 수정 시간 출력 (날짜와 시간 분리)
    struct tm tm;
    localtime_r(&fileStat->st_mtime, &tm);  // thread-safe한 localtime_r 사용
    strftime(lastModDate, sizeof(lastModDate), "%y/%m/%d", &tm);
    strftime(lastModTime, sizeof(lastModTime), "%H:%M", &tm);

    // 창 너비에 따라 출력할 항목을 결정
    if ((winW > 41) && (winW < 50)) {  // 창 너비가 매우 좁으면 파일 이름과 타입만 출력
        if (S_ISDIR(fileStat->st_mode)) {  // 디렉토리
            wattron(win->win, COLOR_PAIR(DIRECTORY));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_S, fileName, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(DIRECTORY));
        } else if (S_ISLNK(fileStat->st_mode)) {  // 링크
            wattron(win->win, COLOR_PAIR(SYMBOLIC));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_S, fileName, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(SYMBOLIC));
        } else if (S_ISREG(fileStat->st_mode)) {  // 일반 파일
            wattron(win->win, COLOR_PAIR(DEFAULT));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_S, fileName, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(DEFAULT));
        } else {  // 나머지
            wattron(win->win, COLOR_PAIR(OTHER));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_S, fileName, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(OTHER));
        }
    } else {  // 창 너비가 넓을 경우 모든 정보를 출력
        if (S_ISDIR(fileStat->st_mode)) {  // 디렉토리
            wattron(win->win, COLOR_PAIR(DIRECTORY));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_L, fileName, fileSize, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(DIRECTORY));
        } else if (S_ISLNK(fileStat->st_mode)) {  // 링크
            wattron(win->win, COLOR_PAIR(SYMBOLIC));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_L, fileName, fileSize, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(SYMBOLIC));
        } else if (S_ISREG(fileStat->st_mode)) {  // 일반 파일
            wattron(win->win, COLOR_PAIR(DEFAULT));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_L, fileName, fileSize, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(DEFAULT));
        } else {  // 나머지
            wattron(win->win, COLOR_PAIR(OTHER));
            mvwprintw(win->win, line + 3, 1, PRINTFILES_FORMAT_L, fileName, fileSize, lastModDate, lastModTime);
            whline(win->win, ' ', winW - getcurx(win->win) - 1);  // 남은 공간 공백 출력
            wattroff(win->win, COLOR_PAIR(OTHER));
        }
    }
}

int calculateWinPos(unsigned int winNo, int *y, int *x, int *h, int *w) {
    int screenW, screenH;
    getmaxyx(stdscr, screenH, screenW);

    // 창의 개수에 따라 분기
    // TODO: 세로줄로 각 창 구분 반영
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
            *y = 2;  // 타이틀 아래
            *x = (winNo == 0) ? 0 : (screenW / 2);  // 첫 번째 창은 왼쪽, 두 번째 창은 오른쪽
            *h = screenH - 5;  // 상단 제목 창과 하단 단축키 창 제외
            *w = screenW / 2;
            return 0;
        case 3:
            *y = 2;  // 타이틀 아래
            *x = winNo * (screenW / 3);  // 각 창은 1/3씩 너비 차지
            *h = screenH - 5;  // 상단 제목 창과 하단 단축키 창 제외
            *w = screenW / 3;
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
        currentWin = winCnt - 1;
    else
        currentWin--;
}

void selectNextWindow(void) {
    if (currentWin == winCnt - 1)
        currentWin = 0;
    else
        currentWin++;
}


// DirEntry 배열 정렬
void sortDirEntries(DirWin *win, int (*compare)(const void *, const void *)) {
    // DirEntry 배열을 만듬 (statEntries와 entryNames를 이용)
    DirEntry entries[*win->totalReadItems];

    for (size_t i = 0; i < *win->totalReadItems; ++i) {
        // entryNames와 statEntries를 DirEntry 배열에 복사
        strncpy(entries[i].entryName, win->entryNames[i], MAX_NAME_LEN);
        entries[i].statEntry = win->statEntries[i];
    }

    // DirEntry 배열을 정렬
    qsort(entries, *win->totalReadItems, sizeof(DirEntry), compare);

    // 정렬된 데이터를 기존의 win->entryNames와 win->statEntries에 다시 복사
    for (size_t i = 0; i < *win->totalReadItems; ++i) {
        strncpy(win->entryNames[i], entries[i].entryName, MAX_NAME_LEN);
        win->statEntries[i] = entries[i].statEntry;
    }
}

void toggleSort(int mask, int shift) {
    SortFlags *flags = &(windows[currentWin].sortFlag);

    // 현재 상태를 추출
    int state = (*flags & mask) >> shift;

    // 만약, 현재 상태가 00이 아니면 이전의 다른 비트들을 모두 00으로 초기화
    if (state == 0) {
        // 다른 비트들은 모두 00으로 초기화
        *flags &= ~SORT_NAME_MASK;  // 이름 기준 비트 초기화
        *flags &= ~SORT_SIZE_MASK;  // 사이즈 기준 비트 초기화
        *flags &= ~SORT_DATE_MASK;  // 날짜 기준 비트 초기화
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
    *flags = (*flags & ~mask) | (state << shift);
}

void applySorting(SortFlags flags, DirWin *win) {
    int (*compareFunc)(const void *, const void *) = NULL;

    if (flags & 0x00) {
        compareFunc = compareByName_Asc;
    }
    // 이름 기준 정렬
    else if ((flags & SORT_NAME_MASK) >> SORT_NAME_SHIFT == 1) {
        compareFunc = compareByName_Asc;
    } else if ((flags & SORT_NAME_MASK) >> SORT_NAME_SHIFT == 2) {
        compareFunc = compareByName_Desc;
    }
    // 크기 기준 정렬
    else if ((flags & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT == 1) {
        compareFunc = compareBySize_Asc;
    } else if ((flags & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT == 2) {
        compareFunc = compareBySize_Desc;
    }
    // 날짜 기준 정렬
    else if ((flags & SORT_DATE_MASK) >> SORT_DATE_SHIFT == 1) {
        compareFunc = compareByDate_Asc;
    } else if ((flags & SORT_DATE_MASK) >> SORT_DATE_SHIFT == 2) {
        compareFunc = compareByDate_Desc;
    }

    // 정렬 함수가 설정되면, DirEntry 배열을 정렬
    if (compareFunc != NULL) {
        sortDirEntries(win, compareFunc);
    }
}


int compareByDate_Asc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }


    // 초 단위 비교
    if (entryA.statEntry.st_mtim.tv_sec < entryB.statEntry.st_mtim.tv_sec)
        return -1;
    if (entryA.statEntry.st_mtim.tv_sec > entryB.statEntry.st_mtim.tv_sec)
        return 1;

    // 초 단위가 같으면 나노초 단위 비교
    if (entryA.statEntry.st_mtim.tv_nsec < entryB.statEntry.st_mtim.tv_nsec)
        return -1;
    if (entryA.statEntry.st_mtim.tv_nsec > entryB.statEntry.st_mtim.tv_nsec)
        return 1;

    return 0;  // 완전히 같음
}
int compareByDate_Desc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }


    // 초 단위 비교
    if (entryA.statEntry.st_mtim.tv_sec < entryB.statEntry.st_mtim.tv_sec)
        return 1;
    if (entryA.statEntry.st_mtim.tv_sec > entryB.statEntry.st_mtim.tv_sec)
        return -1;

    // 초 단위가 같으면 나노초 단위 비교
    if (entryA.statEntry.st_mtim.tv_nsec < entryB.statEntry.st_mtim.tv_nsec)
        return 1;
    if (entryA.statEntry.st_mtim.tv_nsec > entryB.statEntry.st_mtim.tv_nsec)
        return -1;

    return 0;  // 완전히 같음
}

int compareByName_Asc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);
    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }
    // 이름 순 정렬
    return strcmp(entryA.entryName, entryB.entryName);
}
int compareByName_Desc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);
    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }
    // 이름 순 정렬
    return -1 * (strcmp(entryA.entryName, entryB.entryName));
}

int compareBySize_Asc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }

    if (entryA.statEntry.st_size < entryB.statEntry.st_size) return -1;  // a가 b보다 작으면 음수 반환
    if (entryA.statEntry.st_size > entryB.statEntry.st_size) return 1;  // a가 b보다 크면 양수 반환
    return 0;  // a와 b가 같으면 0 반환
}
int compareBySize_Desc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }

    if (entryA.statEntry.st_size < entryB.statEntry.st_size) return 1;  // a가 b보다 작으면 음수 반환
    if (entryA.statEntry.st_size > entryB.statEntry.st_size) return -1;  // a가 b보다 크면 양수 반환
    return 0;  // a와 b가 같으면 0 반환
}
