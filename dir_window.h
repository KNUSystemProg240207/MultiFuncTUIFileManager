#ifndef __DIR_WINDOW_H_INCLUDED__
#define __DIR_WINDOW_H_INCLUDED__

#include <curses.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

typedef unsigned char SortFlags;  // 정렬 상태를 저장하는 플래그 1BTYE


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
    SortFlags sortFlag;
};
typedef struct _DirWin DirWin;

typedef struct {
    char entryName[MAX_NAME_LEN];  // 파일/디렉토리 이름
    struct stat statEntry;  // stat 정보
} DirEntry;

void printFileHeader(DirWin *win, int winH, int winW);
void printFileInfo(DirWin *win, int startIdx, int line, int winW);

/**
 * 새 폴더 표시 창 초기화 (생성)
 *
 * @param statMutex Stat 및 이름 관련 Mutex
 * @param statEntries 항목들의 stat 정보
 * @param entryNames 항목들의 이름
 * @param totalReadItems 현 폴더에서 읽어들인 항목 수
 * @return 성공: (창 초기화 후 창 개수), 실패: -1
 */
int initDirWin(
    pthread_mutex_t *statMutex,
    struct stat *statEntries,
    char (*entryNames)[MAX_NAME_LEN + 1],
    size_t *totalReadItems
);

#define SORT_NAME_MASK 0x03  // 00000011
#define SORT_SIZE_MASK 0x0C  // 00001100
#define SORT_DATE_MASK 0x30  // 00110000

#define SORT_NAME_SHIFT 0
#define SORT_SIZE_SHIFT 2
#define SORT_DATE_SHIFT 4
int compareByName_Asc(const void *a, const void *b);
int compareByName_Desc(const void *a, const void *b);
int compareBySize_Asc(const void *a, const void *b);
int compareBySize_Desc(const void *a, const void *b);
int compareByDate_Asc(const void *a, const void *b);
int compareByDate_Desc(const void *a, const void *b);
void toggleSort(int mask, int shift);
void applySorting(SortFlags flags, DirWin *win);
void sortDirEntries(DirWin *win, int (*compare)(const void *, const void *));
int compareDotEntries(const DirEntry *entryA, const DirEntry *entryB);

/**
 * 폴더 표시 창들 업데이트
 *
 * @return 성공: 0, 실패: -1
 */
int updateDirWins(void);

/**
 * 선택된 창의 커서 한 칸 위로 이동
 */
void moveCursorUp(void);

/**
 * 선택된 창의 커서 한 칸 아래로 이동
 */
void moveCursorDown(void);

/**
 * 왼쪽 창 선택
 */
void selectPreviousWindow(void);

/**
 * 오른쪽 창 선택
 */
void selectNextWindow(void);

#endif
