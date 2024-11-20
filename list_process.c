#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "config.h"
#include "list_process.h"
#include "proc_win.h"
#include "thread_commons.h"

#define PROC_DIR "/proc"
#define STAT_FILENAME "stat"
#define PROC_SCAN_INTERVAL_USEC 500000  // 프로세스 정보를 읽는 주기 (500ms)

static ProcInfo dataArray[MAX_PROCESSES];
static ProcInfo *pointerArray[MAX_PROCESSES];

// 프로세스 정보 읽기 스레드 시작
int startProcThread(pthread_t *newThread, ProcThreadArgs *args) {
    if (startThread(
            newThread, NULL, procThreadMain, NULL,
            PROC_SCAN_INTERVAL_USEC, &args->threadArgs, args
        ) == -1)
        return -1;
    return 0;
}

// 프로세스 정보를 읽는 스레드의 메인 함수
int procThreadMain(void *argsPtr) {
    ProcThreadArgs *args = (ProcThreadArgs *)argsPtr;
    ssize_t readItems;

    // 프로세스 창 상태 확인
    pthread_mutex_lock(&args->procWin->visibleMutex);  // 상태 보호 Mutex 잠금
    if (!args->procWin->isWindowVisible) {
        pthread_mutex_unlock(&args->procWin->visibleMutex);  // 상태 보호 Mutex 해제
        return 0;
    }
    pthread_mutex_unlock(&args->procWin->visibleMutex);

    // 프로세스 정보 읽기
    pthread_mutex_lock(&args->procWin->statMutex);  // 상태 보호 Mutex 잠금
    readProcInfo(args->procWin);
    pthread_mutex_unlock(&args->procWin->statMutex);

    return 0;
}

// proc 디렉토리에서 프로세스 정보를 읽고 procEntries에 저장

int readProcInfo(ProcWin *procWindow) {
    DIR *dir;
    struct dirent *entry;
    size_t procCount = 0;

    // static으로 만들어 처음에만 초기화
    static int isInitialized = 0;
    if (isInitialized == 0) {
        for (size_t i = 0; i < MAX_PROCESSES; i++) {
            pointerArray[i] = &dataArray[i];
        }
        isInitialized = 1;
    }

    // /proc 디렉토리를 열기
    dir = opendir(PROC_DIR);
    if (dir == NULL) {
        perror("Failed to open /proc");
        return -1;
    }

    // /proc 디렉토리 내의 항목 읽기
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) {
            continue;  // 숫자로 시작하지 않으면 건너뜀
        }
        ProcInfo tempProc;

        // 프로세스 수가 최대치를 초과하면 종료
        /*
        if (procCount >= MAX_PROCESSES) {
            fprintf(stderr, "procCount exceeded MAX_PROCESSES\n");
            break;  // 루프를 중단하고 처리 종료
        }
        */

        // 프로세스 정보를 /proc/<PID>/stat 파일에서 읽음
        char statPath[PATH_MAX];
        strcpy(statPath, PROC_DIR);
        strcat(statPath, "/");
        strcat(statPath, entry->d_name);
        strcat(statPath, "/");
        strcat(statPath, STAT_FILENAME);

        FILE *statFile = fopen(statPath, "r");
        if (statFile == NULL) {
            fprintf(stderr, "Failed to open file: %s\n", statPath);
            continue;
        }
        // fscanf로 데이터를 읽어서 구조체에 저장
        if (fscanf(statFile, "%d (%255[^)]) %c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %lu", &tempProc.pid, tempProc.name, &tempProc.state, &tempProc.utime, &tempProc.stime, &tempProc.vsize) != 6) {
            fclose(statFile);
            continue;
        }
        // /proc/<PID>/stat 파일에서 데이터 읽기
        // if (fscanf(statFile, "%d (%255[^)]) %c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %lu", &tempProc.pid, tempProc.name, &tempProc.state, &tempProc.utime, &tempProc.stime, &tempProc.vsize) != 6)
        //     fprintf(stderr, "Failed to parse file: %s\n", statPath);

        /*
        if (fscanf(statFile, "%d (%255[^)]) %c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %lu %lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %*lu",
           &tempProc.pid, tempProc.name, &tempProc.state, &tempProc.utime, &tempProc.stime) != 5)
            fprintf(stderr, "Failed to parse file: %s\n", statPath);
        */
        fclose(statFile);

        if (procCount < MAX_PROCESSES) {  // 배열이 아직 가득 차지 않은 경우 직접 추가
            size_t insertPos = findInsertPosition(procWindow->procEntries, procCount, tempProc.vsize);
            memmove(&procWindow->procEntries[insertPos + 1], &procWindow->procEntries[insertPos], (procCount - insertPos) * sizeof(ProcInfo *));
            dataArray[procCount] = tempProc;
            procWindow->procEntries[insertPos] = &dataArray[procCount];
            procCount++;

        } else if (tempProc.vsize > procWindow->procEntries[0]->vsize) {
            ProcInfo *minItem = procWindow->procEntries[0];
            size_t insertPos = findInsertPosition(procWindow->procEntries, MAX_PROCESSES, tempProc.vsize);
            memmove(&procWindow->procEntries[0], &procWindow->procEntries[1], insertPos * sizeof(ProcInfo *));

            *minItem = tempProc;  // 새로운 데이터로 갱신
            procWindow->procEntries[insertPos] = minItem;
        }
    }
    closedir(dir);

    // 읽어들인 총 프로세스 수를 저장
    procWindow->totalReadItems = procCount;

    return 0;
}

size_t findInsertPosition(ProcInfo *pointerArray[], size_t size, unsigned long vsize) {
    size_t low = 0, high = size;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (pointerArray[mid] == NULL) {  // NULL 방어 코드
            fprintf(stderr, "Null pointer detected at position %zu\n", mid);
            return mid;  // NULL을 발견한 위치에 삽입
        }
        if (pointerArray[mid]->vsize < vsize) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
     if (low > size - 1) {
        return size - 1;
    }
    return low;  // 삽입 위치 반환
}
