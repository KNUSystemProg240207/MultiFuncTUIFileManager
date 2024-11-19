#include <stdlib.h>

#include "config.h"
#include "dir_entry_utils.h"
#include "dir_window.h"

void truncateFileName(char *fileName) {
    size_t len = strlen(fileName);

    if (len > MAX_DISPLAY_LEN) {
        const char *dot = strrchr(fileName, '.');  // 마지막 '.' 찾기
        size_t extLen = dot ? strlen(dot) : 0;  // 확장자의 길이

        // 확장자까지 포함해서 길이가 MAX_DISPLAY_LEN보다 길면 생략
        if (extLen >= MAX_DISPLAY_LEN - 3) {
            fileName[MAX_DISPLAY_LEN - 3] = '\0';
            strcat(fileName, "...");
            return;
        }

        // 확장자를 제외한 앞부분이 MAX_DISPLAY_LEN보다 길면 생략
        size_t prefixLen = MAX_DISPLAY_LEN - extLen - 3;  // '...'을 포함한 앞부분 길이
        fileName[prefixLen] = '\0';  // 그 지점에서 문자열을 잘라냄
        strcat(fileName, "..");  // 생략 기호 추가
        strcat(fileName, dot);  // 확장자 추가
    }
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

// DirEntry 배열 정렬
void sortDirEntries(DirWin *win, int (*compare)(const void *, const void *)) {
    // DirEntry 배열을 만듬 (statEntries와 entryNames를 이용)
    DirEntry entries[*win->totalReadItems];

    for (size_t i = 0; i < *win->totalReadItems; ++i) {
        // entryNames와 statEntries를 DirEntry 배열에 복사
        strncpy(entries[i].entryName, win->bufEntryNames[i], MAX_NAME_LEN);
        entries[i].statEntry = win->bufEntryStat[i];
    }

    // DirEntry 배열을 정렬
    qsort(entries, *win->totalReadItems, sizeof(DirEntry), compare);

    // 정렬된 데이터를 기존의 win->entryNames와 win->statEntries에 다시 복사
    for (size_t i = 0; i < *win->totalReadItems; ++i) {
        strncpy(win->bufEntryNames[i], entries[i].entryName, MAX_NAME_LEN);
        win->bufEntryStat[i] = entries[i].statEntry;
    }
}

/* 정렬 적용 함수 */
void applySorting(SortFlags flags, DirWin *win) {
    int (*compareFunc)(const void *, const void *) = NULL;

    if (flags & 0x00) {  // 디폴트는 이름 오름차순
        compareFunc = compareByName_Asc;
    }
    // 이름 기준 정렬
    else if ((flags & SORT_NAME_MASK) >> SORT_NAME_SHIFT == 1) {
        compareFunc = compareByName_Asc;
    } else if ((flags & SORT_NAME_MASK) >> SORT_NAME_SHIFT == 2) {
        compareFunc = compareByName_Desc;
    }
    // 크기 기준 정렬
    else if ((flags & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT == 1) {
        compareFunc = compareBySize_Asc;
    } else if ((flags & SORT_SIZE_MASK) >> SORT_SIZE_SHIFT == 2) {
        compareFunc = compareBySize_Desc;
    }
    // 날짜 기준 정렬
    else if ((flags & SORT_DATE_MASK) >> SORT_DATE_SHIFT == 1) {
        compareFunc = compareByDate_Asc;
    } else if ((flags & SORT_DATE_MASK) >> SORT_DATE_SHIFT == 2) {
        compareFunc = compareByDate_Desc;
    }

    // 정렬 함수가 설정되면, DirEntry 배열을 정렬
    if (compareFunc != NULL) {
        sortDirEntries(win, compareFunc);
    }
}

int compareByDate_Asc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA.entryName, "..") == 0) return -1;
    if (strcmp(entryB.entryName, "..") == 0) return 1;


    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }


    // 초 단위 비교
    if (entryA.statEntry.st_mtim.tv_sec < entryB.statEntry.st_mtim.tv_sec)
        return -1;
    if (entryA.statEntry.st_mtim.tv_sec > entryB.statEntry.st_mtim.tv_sec)
        return 1;

    // 초 단위가 같으면 나노초 단위 비교
    if (entryA.statEntry.st_mtim.tv_nsec < entryB.statEntry.st_mtim.tv_nsec)
        return -1;
    if (entryA.statEntry.st_mtim.tv_nsec > entryB.statEntry.st_mtim.tv_nsec)
        return 1;

    return 0;  // 완전히 같음
}
int compareByDate_Desc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA.entryName, "..") == 0) return -1;
    if (strcmp(entryB.entryName, "..") == 0) return 1;


    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }


    // 초 단위 비교
    if (entryA.statEntry.st_mtim.tv_sec < entryB.statEntry.st_mtim.tv_sec)
        return 1;
    if (entryA.statEntry.st_mtim.tv_sec > entryB.statEntry.st_mtim.tv_sec)
        return -1;

    // 초 단위가 같으면 나노초 단위 비교
    if (entryA.statEntry.st_mtim.tv_nsec < entryB.statEntry.st_mtim.tv_nsec)
        return 1;
    if (entryA.statEntry.st_mtim.tv_nsec > entryB.statEntry.st_mtim.tv_nsec)
        return -1;

    return 0;  // 완전히 같음
}

int compareByName_Asc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA.entryName, "..") == 0) return -1;
    if (strcmp(entryB.entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }
    // 이름 순 정렬
    return strcmp(entryA.entryName, entryB.entryName);
}
int compareByName_Desc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA.entryName, "..") == 0) return -1;
    if (strcmp(entryB.entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }
    // 이름 순 정렬
    return -1 * (strcmp(entryA.entryName, entryB.entryName));
}

int compareBySize_Asc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA.entryName, "..") == 0) return -1;
    if (strcmp(entryB.entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }

    if (entryA.statEntry.st_size < entryB.statEntry.st_size) return -1;  // a가 b보다 작으면 음수 반환
    if (entryA.statEntry.st_size > entryB.statEntry.st_size) return 1;  // a가 b보다 크면 양수 반환
    return 0;  // a와 b가 같으면 0 반환
}
int compareBySize_Desc(const void *a, const void *b) {
    DirEntry entryA = *((DirEntry *)a);
    DirEntry entryB = *((DirEntry *)b);

    // ".."는 최상단
    if (strcmp(entryA.entryName, "..") == 0) return -1;
    if (strcmp(entryB.entryName, "..") == 0) return 1;

    // 디렉토리는 상단
    if (S_ISDIR(entryA.statEntry.st_mode) && !S_ISDIR(entryB.statEntry.st_mode)) {
        return -1;
    } else if (!S_ISDIR(entryA.statEntry.st_mode) && S_ISDIR(entryB.statEntry.st_mode)) {
        return 1;
    }

    if (entryA.statEntry.st_size < entryB.statEntry.st_size) return 1;  // a가 b보다 작으면 음수 반환
    if (entryA.statEntry.st_size > entryB.statEntry.st_size) return -1;  // a가 b보다 크면 양수 반환
    return 0;  // a와 b가 같으면 0 반환
}
