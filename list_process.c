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
#include "thread_commons.h"

#define PROC_DIR "/proc"
#define STAT_FILENAME "stat"
#define PROC_SCAN_INTERVAL_USEC 500000  // 프로세스 정보를 읽는 주기 (500ms)


static size_t findInsertPosition(Process **readItems, size_t size, unsigned long vsize);


// 프로세스 정보 읽기 스레드 시작
int startProcessThread(pthread_t *newThread, ProcessThreadArgs *args) {
    // clang-format off
    if (startThread(
        newThread, NULL, procThreadMain, NULL,
        PROC_SCAN_INTERVAL_USEC, &args->commonArgs, args
    ) == -1)  // clang-format on
        return -1;
    return 0;
}

// 프로세스 정보를 읽는 스레드의 메인 함수
int procThreadMain(void *argsPtr) {
    ProcessThreadArgs *args = (ProcessThreadArgs *)argsPtr;

    pthread_mutex_lock(&args->commonArgs.statusMutex);  // 상태 보호 Mutex 잠금
    if (args->commonArgs.statusFlags & LISTPROCESS_FLAG_PAUSE_THREAD) {
        args->commonArgs.statusFlags &= ~LISTPROCESS_FLAG_PAUSE_THREAD;
        pthread_cond_wait(&args->commonArgs.resumeThread, &args->commonArgs.statusMutex);
    }
    pthread_mutex_unlock(&args->commonArgs.statusMutex);

    // 프로세스 정보 읽기
    readProcInfo(args);

    return 0;
}

// proc 디렉토리에서 프로세스 정보를 읽고 procEntries에 저장
int readProcInfo(ProcessThreadArgs *args) {
    static Process elements[MAX_PROCESSES];
    static Process *elemPointers[MAX_PROCESSES];

    // /proc 디렉토리를 열기
    DIR *dir = opendir(PROC_DIR);
    if (dir == NULL) {
        perror("Failed to open /proc");
        return -1;
    }

    // /proc 디렉토리 내의 항목 읽기
    size_t readCount = 0;
    struct dirent *entry;
    Process temp;
    char pathBuf[MAX_PATH_LEN + 1];
    while ((entry = readdir(dir)) != NULL) {
        if (!isdigit(entry->d_name[0])) {
            continue;  // 숫자로 시작하지 않으면 건너뜀
        }

        snprintf(pathBuf, MAX_PATH_LEN + 1, "/proc/%.244s/stat", entry->d_name);
        FILE *statFile = fopen(pathBuf, "r");
        if (statFile == NULL) {
            continue;
        }
        // fscanf로 데이터를 읽어서 구조체에 저장
        // clang-format off
        if (fscanf(
            statFile, "%d (%255[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*u %lu",
            &temp.pid, temp.name, &temp.state, &temp.utime, &temp.stime, &temp.vsize
        ) != 6) {  // clang-format on
            fclose(statFile);
            continue;
        }
        fclose(statFile);

        if (readCount < MAX_PROCESSES) {  // 배열이 아직 가득 차지 않은 경우 직접 추가
            size_t insertPos = findInsertPosition(elemPointers, readCount, temp.vsize);
            memmove(&elemPointers[0], &elemPointers[1], insertPos * sizeof(Process *));
            elements[readCount] = temp;
            elemPointers[readCount] = &elements[readCount];
            readCount++;
        } else if (temp.vsize > elemPointers[0]->vsize) {
            size_t insertPos = findInsertPosition(elemPointers, MAX_PROCESSES, temp.vsize);
            memmove(&elemPointers[0], &elemPointers[1], insertPos * sizeof(Process *));
            *elemPointers[0] = temp;  // 새로운 데이터로 갱신
            elemPointers[insertPos] = elemPointers[0];
        }
    }
    closedir(dir);

    // 읽어들인 총 프로세스 수를 저장
    pthread_mutex_lock(&args->entriesMutex);  // 상태 보호 Mutex 잠금
    args->totalReadItems = readCount;
    for (int i = 0; i < readCount; i++)
        args->processEntries[i] = *elemPointers[i];
    pthread_mutex_unlock(&args->entriesMutex);

    return 0;
}

size_t findInsertPosition(Process **readItems, size_t size, unsigned long vsize) {
    size_t low = 0, high = size;
    while (low < high) {
        size_t mid = (low + high) / 2;
        // if (readItems[mid] == NULL) {  // NULL 방어 코드
        //     fprintf(stderr, "Null pointer detected at position %zu\n", mid);
        //     return mid;  // NULL을 발견한 위치에 삽입
        // }
        if (vsize > readItems[mid]->vsize) {
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
