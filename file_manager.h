#ifndef __FILE_MANAGER_H_INCLUDED__
#define __FILE_MANAGER_H_INCLUDED__

#include <curses.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PATH_LEN 4096
#define MAX_NAME_LEN 256
#define MAX_FILES 1024
//현재 경로 관리를 위해 current_path를 전역으로 접근 가능하게 수정:
extern char current_path[MAX_PATH_LEN];

typedef struct {
    char name[MAX_NAME_LEN];
    mode_t mode;
    off_t size;
    time_t mtime;
    int is_dir;
} DirEntry;

typedef enum {
    COPY,
    MOVE,
    DELETE
} FileOperation;

// 함수 선언들
int initFileManager(void);
void cleanupFileManager(void);
int executeFileOperation(FileOperation op);
int getPathInput(const char* prompt, char* buffer, size_t buflen);

#endif