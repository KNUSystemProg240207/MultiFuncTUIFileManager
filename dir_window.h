#ifndef __DIR_WINDOW_H_INCLUDED__
#define __DIR_WINDOW_H_INCLUDED__

#include <curses.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>

typedef unsigned char SortFlags;  // 정렬 상태를 저장하는 플래그 1BTYE
#define SORT_NAME_MASK 0x03  // 00000011
#define SORT_SIZE_MASK 0x0C  // 00001100
#define SORT_DATE_MASK 0x30  // 00110000

#define SORT_NAME_SHIFT 0
#define SORT_SIZE_SHIFT 2
#define SORT_DATE_SHIFT 4
void toggleSort(int mask, int shift);


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

#endif
