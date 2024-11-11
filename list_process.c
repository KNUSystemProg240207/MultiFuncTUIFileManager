#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "proc_win.h"

#define PROC_DIR "/proc"
#define STAT_FILENAME "stat"


// 프로세스 정보를 읽어와 procEntries 배열에 저장
int readProcInfo(ProcWin *procWindow) {
    DIR *dir;
    struct dirent *entry;
    size_t procCount = 0;

    // /proc 디렉토리를 열기
    dir = opendir(PROC_DIR);
    if (dir == NULL) {
        perror("Failed to open /proc");
        return -1;
    }

    // /proc 디렉토리 내의 항목 읽기
    while ((entry = readdir(dir)) != NULL && procCount < MAX_PROCESSES) {
        // PID를 procEntries 배열에 저장
        procWindow->procEntries[procCount].pid = atoi(entry->d_name);

        // 프로세스 정보를 /proc/<PID>/stat 파일에서 읽음
        char statPath[PATH_MAX];
        strcpy(statPath, PROC_DIR);
        strcat(statPath, "/");
        strcat(statPath, entry->d_name);
        strcat(statPath, "/");
        strcat(statPath, STAT_FILENAME);

        FILE *statFile = fopen(statPath, "r");
        if (statFile == NULL) {
            continue;
        }

        // /proc/<PID>/stat 파일에서 데이터 읽기
		fscanf(statFile, "%d (%255[^)]) %c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu %lu %ld %*ld %*ld %*ld %*ld %*ld, %*llu, %lu",
               &procWindow->procEntries[procCount].pid,
               procWindow->procEntries[procCount].name,
               &procWindow->procEntries[procCount].state,
               &procWindow->procEntries[procCount].utime,
               &procWindow->procEntries[procCount].stime,
               &procWindow->procEntries[procCount].vsize);
        fclose(statFile);
    }

    closedir(dir);

    // 읽어들인 총 프로세스 수를 저장
    procWindow->totalReadItems = procCount;

    return 0;
}
