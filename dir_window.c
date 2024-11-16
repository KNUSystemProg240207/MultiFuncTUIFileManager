#include <curses.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "colors.h"
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
 *   일반적으로, 디렉토리에 있는 파일, 폴더의 수와 같음
 *   단, buffer 공간 부족한 경우, 최대 buffer 길이
 * @var _DirWin::lineMovementEvent (bit field) 창별 줄 이동 Event 저장
 *   Event당 2bit (3종류 Event 존재: 이벤트 없음(0b00), 위로 이동(0b10), 아래로 이동(0b11)) -> 최대 32개 Event 저장
 *   (Mutex 잠그지 않고 이동 처리 가능하게 함)
 */
struct _DirWin {
    WINDOW *win;
    unsigned int order;
    size_t currentPos;
    pthread_mutex_t *statMutex;
    struct stat *statEntries;
    char (*entryNames)[MAX_NAME_LEN + 1];
    size_t *totalReadItems;
    uint64_t lineMovementEvent;
    char (*entryTimes)[DATETIME_LEN + 1];
};
typedef struct _DirWin DirWin;


static DirWin windows[MAX_DIRWINS];  // 각 창의 runtime 정보 저장
static unsigned int winCnt;  // 창 개수
static unsigned int currentWin;  // 현재 창의 Index

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


int initDirWin(
    pthread_mutex_t *statMutex,
    struct stat *statEntries,
    char (*entryNames)[MAX_NAME_LEN + 1],
    size_t *totalReadItems,
    char (*entryTimes)[DATETIME_LEN + 1]
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
        .entryTimes = entryTimes
    };
    return winCnt;
}

int compareByName(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}
int compareByType(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

int compareByDate(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}
int compareBySize(const void *a, const void *b) {
    struct stat statA = *((struct stat *)a);
    struct stat statB = *((struct stat *)b);

    if (statA.st_size < statB.st_size) return -1;  // a가 b보다 작으면 음수 반환
    if (statA.st_size > statB.st_size) return 1;  // a가 b보다 크면 양수 반환
    return 0;  // a와 b가 같으면 0 반환
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
        winH -= 2;  // 최대 출력 가능한 라인 넘버 -2
        // winW = 70;  // 윈도우 크기 확인용

        // 정렬 부분, 아직 정리 x
        qsort(win->entryNames, *win->totalReadItems, sizeof(win->entryNames[0]), compareByName);  // 이름 정렬
        // qsort(win->statEntries, *win->totalReadItems, sizeof(*win->statEntries), compareBySize);
        // qsort(win->entryTimes, *win->totalReadItems, sizeof(win->entryTimes[0]), compareByName);

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

        currentLine = win->currentPos - startIdx;  // 역상으로 출력할, 현재 선택된 줄

        /* 디렉토리 출력 */
        for (line = 0; line < itemsToPrint; line++) {  // 항목 있는 공간: 출력
            if (winNo == currentWin && line == currentLine)  // 선택된 것 역상으로 출력
                wattron(win->win, A_REVERSE);
            printFileInfo(win, startIdx, line, winW);
            if (winNo == currentWin && line == currentLine)
                wattroff(win->win, A_REVERSE);  // 이걸 SELECT로 바꾸면 생기는 분제, 기존색깔로 바꿔줘야 하는데, 안바꾸잖아
        }

        /* 버그 나서(마지막 줄만 whline 미적용) 임시로 주석 달아놓음 */
        wclrtobot(win->win);  // 아래 남는 공간: 지움

        wrefresh(win->win);  // 창 새로 그림
        pthread_mutex_unlock(win->statMutex);
    }

    return 0;
}


// 파일 확장자를 확인하는 함수
const char *getFileExtension(const char *fileName) {
    const char *dot = strrchr(fileName, '.');  // 파일명에서 마지막 '.'을 찾음
    if (!dot || dot == fileName) {
        return "";  // 확장자가 없는 경우
    }
    return dot + 1;  // 확장자 반환
}

void printFileHeader(DirWin *win, int winH, int winW) {
    wattron(win->win, COLOR_PAIR(HEADER));
    // winW 값에 따라 헤더를 결정
    if (winW < 40) {
        // 창이 3개일 때: 간단한 헤더
        mvwaddstr(win->win, 0, 0, "      FILE NAME      |   LAST MODIFIED   |");
    } else if (winW < 50) {
        // 창이 2개일 때: 파일 이름, 파일 타입, 날짜만 포함된 헤더
        mvwaddstr(win->win, 0, 0, "      FILE NAME      |   TYPE   |   LAST MODIFIED   |");
    } else {
        // 창이 1개일 때: 모든 정보가 포함된 상세 헤더
        mvwaddstr(win->win, 0, 0, "      FILE NAME      |   TYPE   |  SIZE  |    LAST MODIFIED    |");
    }
    whline(win->win, ' ', winW - getcurx(win->win));  // 현재 줄의 남은 공간: 공백으로 덮어씀 (역상 출력 위함)
    wattroff(win->win, COLOR_PAIR(HEADER));
    mvhline(3, 0, 0, COLS);  // 구분선
}
// 주어진 텍스트를 주어진 너비에 맞게 가운데 정렬한 문자열을 반환하는 함수
char *centerText(const char *text, int width) {
    static char centeredText[256];  // 정렬된 텍스트를 저장할 배열 (길이를 충분히 크게 설정)

    int textLength = strlen(text);  // 텍스트의 길이 계산
    int padding = (width - textLength) / 2;  // 텍스트 양 옆에 넣을 여백 계산

    // 여백이 음수일 수 없으므로 최소값을 0으로 설정
    if (padding < 0) padding = 0;

    // 텍스트를 배열의 가운데에 배치
    // sprintf를 사용하여 텍스트를 가운데로 배치하고 나머지는 공백으로 채운다.
    snprintf(centeredText, width + 1, "%*s%s%*s", padding, "", text, padding, "");

    return centeredText;  // 가운데 정렬된 텍스트 반환
}


void getFileType(DirWin *win, struct stat *fileStat, char *fileType, char *fileName) {
    const char *extension = getFileExtension(fileName);
    if (S_ISDIR(fileStat->st_mode)) {  // 디렉토리
        wattron(win->win, COLOR_PAIR(DIRECTORY));
        strcpy(fileType, "DIR");
    } else if (S_ISLNK(fileStat->st_mode)) {  // 링크
        wattron(win->win, COLOR_PAIR(SYMBOLIC));
        strcpy(fileType, "LNK");
    } else if (S_ISREG(fileStat->st_mode)) {  // 일반 파일
        if (extension) {
            switch (extension[0]) {  // 체크
                case 'e':  // 실행파일
                case 'o':
                    wattron(win->win, COLOR_PAIR(EXE));
                    if (strcmp(extension, "exe") == 0 || strcmp(extension, "out") == 0 || strcmp(extension, "bin") == 0)
                        strcpy(fileType, "EXE");
                    break;
                case 't':  // 텍스트
                case 'l':  // 텍스트
                    wattron(win->win, COLOR_PAIR(TXT));
                    if (strcmp(extension, "txt") == 0 || strcmp(extension, "log") == 0)
                        strcpy(fileType, "TXT");
                    break;
                case 'c':  // 소스코드
                case 'h':
                    wattron(win->win, COLOR_PAIR(SRC));
                    if (strcmp(extension, "c") == 0 || strcmp(extension, "cpp") == 0 || strcmp(extension, "h") == 0)
                        strcpy(fileType, "SRC");
                    break;
                case 'j':  // 이미지
                case 'p':
                    wattron(win->win, COLOR_PAIR(IMG));
                    if (strcmp(extension, "jpg") == 0 || strcmp(extension, "png") == 0)
                        strcpy(fileType, "IMG");
                    break;
                    // 필요하면 더 추가
                default:
                    wattron(win->win, COLOR_PAIR(DEFAULT));
                    strcpy(fileType, "FILE");
            }
        } else {
            wattron(win->win, COLOR_PAIR(DEFAULT));
            strcpy(fileType, "FILE");
        }
    } else {
        wattron(win->win, COLOR_PAIR(OTHER));
        strcpy(fileType, "OTHER");
    }
}

void printFileInfo(DirWin *win, int startIdx, int line, int winW) {
    struct stat *fileStat = win->statEntries + (startIdx + line);
    char *fileName = win->entryNames[startIdx + line];
    size_t fileSize = fileStat->st_size;
    char *fileTime = win->entryTimes[startIdx + line];
    char fileType[20];
    char lastModDate[20];
    char lastModTime[20];

    // 시간 분리
    // size_t date_size = sizeof("%b %d") / sizeof(char);
    // size_t time_size = sizeof("%h:%M%p") / sizeof(char);
    size_t date_size = sizeof("0000-00-00") / sizeof(char);
    size_t time_size = sizeof("00:00:00") / sizeof(char);

    strncpy(lastModDate, fileTime, date_size);
    strncpy(lastModTime, (fileTime + date_size), time_size);
    lastModDate[date_size] = '\0';
    lastModTime[time_size] = '\0';

    getFileType(win, fileStat, fileType, fileName);

    // 창 너비에 따라 출력할 항목을 결정
    if (winW < 40) {
        // 창 너비가 매우 좁으면 파일 이름과 타입만 출력
        mvwprintw(win->win, line + 2, 0, "%-21s %10s %s", fileName, lastModDate, lastModTime);
    } else if (winW < 50) {
        // 창 너비가 중간일 경우 파일 이름, 타입, 마지막 수정 날짜만 출력
        mvwprintw(win->win, line + 2, 0, "%-21s %10s %10s %s", fileName, fileType, lastModDate, lastModTime);
    } else {
        // 창 너비가 넓을 경우 모든 정보를 출력
        mvwprintw(win->win, line + 2, 0, "%-21s %10s %8zu %12s %s", fileName, centerText(fileType, 10), fileSize, lastModDate, lastModTime);
    }
    whline(win->win, ' ', winW - getcurx(win->win));  // 현재 줄의 남은 공간: 공백으로 덮어씀 (역상 출력 위함)
    wattron(win->win, COLOR_PAIR(DEFAULT));
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

/*
TODO
1. 파일 타입 정렬(DirWin 구조체에 따로 저장해둬야 할 수 있음)
2. 타입별로 색깔 다르게 하는 거 좀 가시성 좋게,
3. 타입별로 출력 다르게 하는 거, 윈도우 크기마다 출력 다르게 하는 거 좀 정리해서
좀 보기 좋게 바꾸기 윈도우 크기를 먼저 체크해보던가, 잘
4. midnight commander처럼 window에 박스를 친다던가?
5. 윈도우 패널화 시키기
*/
