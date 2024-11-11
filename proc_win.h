#ifndef PROC_WIN_H
#define PROC_WIN_H

#include <curses.h>

#define MAX_PROCESSES 256  // 한 번에 표시 가능한 최대 프로세스 수
#define MAX_NAME_LEN 256

typedef struct {
    pid_t pid;                   // 프로세스 ID
    char name[MAX_NAME_LEN + 1]; // 프로세스 이름
    char state;                  // 프로세스 상태
    unsigned long vsize;         // 가상 메모리 사용량
    unsigned long utime;         // 사용자 모드에서의 CPU 시간
    unsigned long stime;         // 커널 모드에서의 CPU 시간
} ProcInfo;

struct _ProcWin {
    WINDOW *win;
    size_t currentPos;
    pthread_mutex_t statMutex;
    ProcInfo procEntries[MAX_PROCESSES];  // 프로세스 정보 배열
    size_t totalReadItems;  // 읽어들인 총 프로세스 수
    uint64_t lineMovementEvent;  // 라인 이동 이벤트
};
typedef struct _ProcWin ProcWin;
#endif
