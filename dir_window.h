#ifndef __DIR_WINDOW_H_INCLUDED__
#define __DIR_WINDOW_H_INCLUDED__

#include <curses.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

/* SortFlags 정의 */
/**
 * @typedef SortFlags
 * 정렬 상태를 저장하는 플래그 (1BYTE 사용).
 * 비트마스크를 이용해 이름, 크기, 날짜 정렬 상태를 나타냅니다.
 */
typedef unsigned char SortFlags;

/**
 * @struct _DirWin
 * Directory Window의 정보 저장
 *
 * @var _DirWin::win WINDOW 구조체
 * @var _DirWin::order Directory 창 순서 (가장 왼쪽=0) ( [0, MAX_DIRWINS) )
 * @var _DirWin::currentPos 현재 선택된 Element
 * @var _DirWin::bufMutex 현 폴더 항목들의 Stat 및 이름 관련 Mutex
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
    pthread_mutex_t *bufMutex;
    struct stat *bufEntryStat;
    char (*bufEntryNames)[MAX_NAME_LEN + 1];
    size_t *totalReadItems;
    uint64_t lineMovementEvent;
    SortFlags sortFlag;
};
typedef struct _DirWin DirWin;


/* DirEntry 구조체 */
/**
 * @typedef DirEntry
 * 디렉토리 항목의 이름 및 파일 정보를 저장하는 구조체.
 *
 * @member entryName 파일/디렉토리 이름 (최대 MAX_NAME_LEN 길이)
 * @member statEntry 파일/디렉토리의 stat 정보
 */
typedef struct {
    char entryName[MAX_NAME_LEN];  // 파일/디렉토리 이름
    struct stat statEntry;  // stat 정보
} DirEntry;

/** 이름 정렬 상태를 나타내는 비트마스크 */
#define SORT_NAME_MASK 0x03  // 00000011
/** 크기 정렬 상태를 나타내는 비트마스크 */
#define SORT_SIZE_MASK 0x0C  // 00001100
/** 날짜 정렬 상태를 나타내는 비트마스크 */
#define SORT_DATE_MASK 0x30  // 00110000

/** 이름 정렬 상태를 위한 비트 쉬프트 */
#define SORT_NAME_SHIFT 0
/** 크기 정렬 상태를 위한 비트 쉬프트 */
#define SORT_SIZE_SHIFT 2
/** 날짜 정렬 상태를 위한 비트 쉬프트 */
#define SORT_DATE_SHIFT 4

/**
 * 새 폴더 표시 창 초기화 (생성)
 *
 * @param bufMutex Stat 및 이름 관련 Mutex
 * @param bufEntryStat 항목들의 stat 정보
 * @param bufEntryNames 항목들의 이름
 * @param totalReadItems 현 폴더에서 읽어들인 항목 수
 * @return 성공: (창 초기화 후 창 개수), 실패: -1
 */
int initDirWin(
    pthread_mutex_t *bufMutex,
    struct stat *bufEntryStat,
    char (*bufEntryNames)[MAX_NAME_LEN + 1],
    size_t *totalReadItems
);

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

/**
 * 현재 창의 선택된 폴더 Index 리턴
 *
 * @return 폴더 선택됨: (현재 선택의 Index), 폴더 아님: -1
 */
ssize_t getCurrentSelectedDirectory(void);

/**
 * 현재 창의 선택된 폴더 Index 변경
 */
void setCurrentSelectedDirectory(size_t index);

/**
 * 현재 선택된 창 번호 리턴
 *
 * @return 현재 선택된 창 번호
 */
unsigned int getCurrentWindow(void);


/**
 * 윈도우에 패널을 초기화합니다.
 *
 * @param win 패널을 생성할 대상 WINDOW 포인터
 * @return 성공 시 0, 실패 시 -1
 *
 * @details
 * - 주어진 WINDOW 객체에 새로운 패널을 생성하여 연결합니다.
 * - 생성된 패널은 패널 스택에서 초기화되며, 나중에 순서 조정 및 갱신이 가능합니다.
 * - 실패 시 적절한 에러 처리를 통해 반환 값을 확인해야 합니다.
 */
int initPanelForWindow(WINDOW *win);

/**
 * 모든 패널의 순서를 갱신하고 화면에 적용합니다.
 *
 * @details
 * - 패널 스택을 기준으로 패널의 순서를 재조정하고, 화면에 최신 상태를 출력합니다.
 * - 이 함수는 여러 패널이 겹치는 UI 환경에서 화면 갱신을 보장합니다.
 */
void refreshPanels();

/**
 * 디렉토리 창의 파일 목록 상단 헤더를 출력합니다.
 *
 * @param win 디렉토리 표시 창
 * @param winH 창의 높이
 * @param winW 창의 너비
 */
void printFileHeader(DirWin *win, int winH, int winW);

/**
 * 디렉토리 항목 정보를 출력합니다.
 *
 * @param win 디렉토리 표시 창
 * @param startIdx 출력 시작 인덱스
 * @param line 출력할 줄 번호
 * @param winW 창의 너비
 */
void printFileInfo(DirWin *win, int startIdx, int line, int winW);

/**
 * 정렬 상태를 토글합니다.
 *
 * @param mask 적용할 비트마스크 (SORT_NAME_MASK, SORT_SIZE_MASK, SORT_DATE_MASK 중 하나)
 * @param shift 비트 쉬프트 값 (SORT_NAME_SHIFT, SORT_SIZE_SHIFT, SORT_DATE_SHIFT 중 하나)
 */
void toggleSort(int mask, int shift);

#endif
