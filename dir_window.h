#ifndef __DIR_WINDOW_H_INCLUDED__
#define __DIR_WINDOW_H_INCLUDED__

#include <curses.h>
#include <pthread.h>
#include <sys/stat.h>

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

/**
 * 폴더 표시 창들 업데이트
 * 
 * @return 성공: 0, 실패: -1
 */
int updateDirWins(void);

#endif
