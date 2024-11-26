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


static long pageSize;  // 메모리 Page Size: Thread 시작 전 알아내야 함

static size_t findInsertPosition(Process **readItems, size_t size, unsigned long rsize);


// 프로세스 정보 읽기 스레드 시작
int startProcessThread(pthread_t *newThread, ProcessThreadArgs *args) {
    pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize == -1)
        return -1;
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
            statFile, "%d (%255[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %*d %*d %*u %*u %ld",
            &temp.pid, temp.name, &temp.state, &temp.utime, &temp.stime, &temp.rsize
        ) != 6) {  // clang-format on
            fclose(statFile);
            continue;
        }
        temp.rsize *= pageSize;
        fclose(statFile);

        if (readCount < MAX_PROCESSES) {  // 배열이 아직 가득 차지 않은 경우 직접 추가
            size_t insertPos = findInsertPosition(elemPointers, readCount, temp.rsize);
            if (insertPos != readCount) {  // memmove가 필요할 때만 호출, insertPos가 127일 때 에러발생
                memmove(&elemPointers[insertPos + 1], &elemPointers[insertPos], (readCount - insertPos) * sizeof(Process *));
            }
            elements[readCount] = temp;
            elemPointers[insertPos] = &elements[readCount];
            readCount++;
        } else if (temp.rsize > elemPointers[0]->rsize) {
            Process* pointerBuffer = elemPointers[0];
            size_t insertPos = findInsertPosition(elemPointers, MAX_PROCESSES - 1, temp.rsize); // readCount와 동일하게 실제 요소 수를 인자로 주기 위해 MAX_PROCESSES-1
            memmove(&elemPointers[0], &elemPointers[1], insertPos * sizeof(Process *));
            *pointerBuffer = temp;  // 새로운 데이터로 갱신
            elemPointers[insertPos] = pointerBuffer;
        }
    }
    closedir(dir);

    // 공유 변수에 읽어들인 정보 쓰기
    pthread_mutex_lock(&args->entriesMutex);  // 상태 보호 Mutex 잠금
    args->totalReadItems = readCount;
    for (int i = 0; i < readCount; i++)
        args->processEntries[i] = *elemPointers[i];
    pthread_mutex_unlock(&args->entriesMutex);

    return 0;
}

size_t findInsertPosition(Process **readItems, size_t size, unsigned long rsize) {
    size_t low = 0, high = size;
    if(size == 0)
        return 0;

    // 이진 탐색
    while (low < high) {
        size_t mid = (low + high) / 2;
        if (rsize > readItems[mid]->rsize) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    if (size == MAX_PROCESSES) {
        if (rsize > readItems[low]->rsize && low != size) {
            return low + 1;  // 배열 끝에 삽입될 의도, 삽입될 위치가 끝일 경우 제외
        }
        return low;  // 현재 위치에서 대체
    }

    // 배열이 가득 차지 않은 경우
    if (rsize > readItems[low]->rsize || low == 0) {
        return low;  // 배열 끝에 삽입, 삽입될 위치가 0인 경우
    }
    return low - 1;  // 마지막 요소를 이동시켜 삽입
}

