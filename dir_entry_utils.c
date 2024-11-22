#include <stdlib.h>

#include "config.h"
#include "dir_entry_utils.h"
#include "dir_window.h"
#include "thread_commons.h"

char *truncateFileName(char *fileName) {
    static char nameBuf[MAX_NAME_LEN + 1];
    size_t len = strlen(fileName);

    strncpy(nameBuf, fileName, MAX_NAME_LEN);
    nameBuf[MAX_NAME_LEN] = '\0';

    if (len > MAX_DISPLAY_LEN) {
        const char *dot = strrchr(fileName, '.');  // 마지막 '.' 찾기
        size_t extLen = dot ? strlen(dot) : 0;  // 확장자의 길이

        if (extLen >= MAX_DISPLAY_LEN - 3) {
            // 확장자 길이가 너무 김
            nameBuf[MAX_DISPLAY_LEN - 3] = '\0';
            strcat(nameBuf, "...");
            return nameBuf;
        } else {
            // 확장자 길이는 길지 않음
            size_t prefixLen = MAX_DISPLAY_LEN - extLen - 3;  // '...'을 포함한 앞부분 길이
            nameBuf[prefixLen] = '\0';  // 그 지점에서 문자열을 잘라냄
            strcat(nameBuf, "..");  // 생략 기호 추가
            if (dot != NULL)
                strcat(nameBuf, dot);  // 확장자 추가
        }
    }

    return nameBuf;
}

int isHidden(const char *fileName) {
    return fileName[0] == '.' && strcmp(fileName, ".") != 0 && strcmp(fileName, "..") != 0;
}
int isImageFile(const char *fileName) {
    const char *extensions[] = { ".jpg", ".jpeg", ".png", ".gif", ".bmp" };
    size_t numExtensions = sizeof(extensions) / sizeof(extensions[0]);

    const char *dot = strrchr(fileName, '.');  // 마지막 '.' 위치 찾기
    if (dot) {
        for (size_t i = 0; i < numExtensions; i++) {
            if (strcasecmp(dot, extensions[i]) == 0) {  // 대소문자 구분 없이 비교
                return 1;
            }
        }
    }
    return 0;
}

int isEXE(const char *fileName) {
    const char *extensions[] = { ".exe", ".out" };
    size_t numExtensions = sizeof(extensions) / sizeof(extensions[0]);

    const char *dot = strrchr(fileName, '.');  // 마지막 '.' 위치 찾기
    if (dot) {
        for (size_t i = 0; i < numExtensions; i++) {
            if (strcasecmp(dot, extensions[i]) == 0) {  // 대소문자 구분 없이 비교
                return 1;
            }
        }
    }
    return 0;
}

/* 정렬 적용 함수 */
void applySorting(DirEntry *dirEntries, uint16_t flags, size_t totalReadItems) {
    if (!dirEntries || totalReadItems == 0) {
        fprintf(stderr, "Invalid input to applySorting: dirEntries=%p, totalReadItems=%zu\n", dirEntries, totalReadItems);
        return;
    }

    int (*compareFunc)(const void *, const void *) = NULL;

    // 정렬 기준과 방향을 결정하는 비트 추출
    uint16_t criterion = flags & SORT_CRITERION_MASK;  // 정렬 기준
    uint16_t direction = flags & SORT_DIRECTION_BIT;  // 정렬 방향

    // 이름 기준 정렬
    if (criterion == SORT_NAME) {
        compareFunc = (direction == 0) ? cmpNameAsc : cmpNameDesc;
    }
    // 크기 기준 정렬
    else if (criterion == SORT_SIZE) {
        compareFunc = (direction == 0) ? cmpSizeAsc : cmpSizeDesc;
    }
    // 날짜 기준 정렬
    else if (criterion == SORT_DATE) {
        compareFunc = (direction == 0) ? cmpDateAsc : cmpDateDesc;
    }

    // 정렬 함수가 설정되었으면, DirEntry 배열을 정렬
    if (compareFunc != NULL) {
        qsort(dirEntries, totalReadItems, sizeof(DirEntry), compareFunc);
    } else {
        fprintf(stderr, "Invalid sorting flags: flags=%u (criterion=%u, direction=%u)\n", flags, criterion, direction);
    }
}

int cmpDateAsc(const void *a, const void *b) {
    DirEntry *entryA = ((DirEntry *)a);
    DirEntry *entryB = ((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA->entryName, "..") == 0) return -1;
    if (strcmp(entryB->entryName, "..") == 0) return 1;


    // 디렉토리는 상단
    if (S_ISDIR(entryA->statEntry.st_mode) && !S_ISDIR(entryB->statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA->statEntry.st_mode) && S_ISDIR(entryB->statEntry.st_mode)) {
        return 1;
    }


    // 초 단위 비교
    if (entryA->statEntry.st_mtim.tv_sec < entryB->statEntry.st_mtim.tv_sec)
        return -1;
    if (entryA->statEntry.st_mtim.tv_sec > entryB->statEntry.st_mtim.tv_sec)
        return 1;

    // 초 단위가 같으면 나노초 단위 비교
    if (entryA->statEntry.st_mtim.tv_nsec < entryB->statEntry.st_mtim.tv_nsec)
        return -1;
    if (entryA->statEntry.st_mtim.tv_nsec > entryB->statEntry.st_mtim.tv_nsec)
        return 1;

    return strcmp(entryA->entryName, entryB->entryName);  // 완전히 같으면 이름 비교
}
int cmpDateDesc(const void *a, const void *b) {
    DirEntry *entryA = ((DirEntry *)a);
    DirEntry *entryB = ((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA->entryName, "..") == 0) return -1;
    if (strcmp(entryB->entryName, "..") == 0) return 1;


    // 디렉토리는 상단
    if (S_ISDIR(entryA->statEntry.st_mode) && !S_ISDIR(entryB->statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA->statEntry.st_mode) && S_ISDIR(entryB->statEntry.st_mode)) {
        return 1;
    }


    // 초 단위 비교
    if (entryA->statEntry.st_mtim.tv_sec < entryB->statEntry.st_mtim.tv_sec)
        return 1;
    if (entryA->statEntry.st_mtim.tv_sec > entryB->statEntry.st_mtim.tv_sec)
        return -1;

    // 초 단위가 같으면 나노초 단위 비교
    if (entryA->statEntry.st_mtim.tv_nsec < entryB->statEntry.st_mtim.tv_nsec)
        return 1;
    if (entryA->statEntry.st_mtim.tv_nsec > entryB->statEntry.st_mtim.tv_nsec)
        return -1;

    return -1 * (strcmp(entryA->entryName, entryB->entryName));  // 완전히 같으면 이름 비교
}

int cmpNameAsc(const void *a, const void *b) {
    DirEntry *entryA = ((DirEntry *)a);
    DirEntry *entryB = ((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA->entryName, "..") == 0) return -1;
    if (strcmp(entryB->entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA->statEntry.st_mode) && !S_ISDIR(entryB->statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA->statEntry.st_mode) && S_ISDIR(entryB->statEntry.st_mode)) {
        return 1;
    }
    // 이름 순 정렬
    return strcmp(entryA->entryName, entryB->entryName);
}
int cmpNameDesc(const void *a, const void *b) {
    DirEntry *entryA = ((DirEntry *)a);
    DirEntry *entryB = ((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA->entryName, "..") == 0) return -1;
    if (strcmp(entryB->entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA->statEntry.st_mode) && !S_ISDIR(entryB->statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA->statEntry.st_mode) && S_ISDIR(entryB->statEntry.st_mode)) {
        return 1;
    }
    // 이름 순 정렬
    return -1 * (strcmp(entryA->entryName, entryB->entryName));
}

int cmpSizeAsc(const void *a, const void *b) {
    DirEntry *entryA = ((DirEntry *)a);
    DirEntry *entryB = ((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA->entryName, "..") == 0) return -1;
    if (strcmp(entryB->entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA->statEntry.st_mode) && !S_ISDIR(entryB->statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA->statEntry.st_mode) && S_ISDIR(entryB->statEntry.st_mode)) {
        return 1;
    }

    if (entryA->statEntry.st_size < entryB->statEntry.st_size) return -1;  // a가 b보다 작으면 음수 반환
    if (entryA->statEntry.st_size > entryB->statEntry.st_size) return 1;  // a가 b보다 크면 양수 반환
    return (strcmp(entryA->entryName, entryB->entryName));  // a와 b가 같으면 이름 비교
}
int cmpSizeDesc(const void *a, const void *b) {
    DirEntry *entryA = ((DirEntry *)a);
    DirEntry *entryB = ((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA->entryName, "..") == 0) return -1;
    if (strcmp(entryB->entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA->statEntry.st_mode) && !S_ISDIR(entryB->statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA->statEntry.st_mode) && S_ISDIR(entryB->statEntry.st_mode)) {
        return 1;
    }

    if (entryA->statEntry.st_size < entryB->statEntry.st_size) return 1;  // a가 b보다 작으면 음수 반환
    if (entryA->statEntry.st_size > entryB->statEntry.st_size) return -1;  // a가 b보다 크면 양수 반환
    return -1 * (strcmp(entryA->entryName, entryB->entryName));  // a와 b가 같으면 이름 비교
}
