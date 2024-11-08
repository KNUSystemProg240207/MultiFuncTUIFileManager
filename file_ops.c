#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <curses.h>
#include <ctype.h>

#include "file_ops.h"
#include "commons.h"
#include "commons.h"

#define BUFFER_SIZE 4096

static int copyRegularFile(const char *src, const char *dst);
static int copyDirectory(const char *src, const char *dst);

int copyFile(const char *src, const char *dst) {
    struct stat st;
    
    if (stat(src, &st) == -1) {
        return -1;
    }

    if (S_ISREG(st.st_mode)) {
        return copyRegularFile(src, dst);
    } else if (S_ISDIR(st.st_mode)) {
        return copyDirectory(src, dst);
    }
    
    return -1;
}

static int copyRegularFile(const char *src, const char *dst) {
    int fd_src, fd_dst;
    ssize_t nread;
    char buf[BUFFER_SIZE];

    fd_src = open(src, O_RDONLY);
    if (fd_src == -1) return -1;

    fd_dst = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_dst == -1) {
        close(fd_src);
        return -1;
    }

    while ((nread = read(fd_src, buf, BUFFER_SIZE)) > 0) {
        if (write(fd_dst, buf, nread) != nread) {
            close(fd_src);
            close(fd_dst);
            return -1;
        }
    }

    close(fd_src);
    close(fd_dst);
    return 0;
}

static int copyDirectory(const char *src, const char *dst) {
    // 디렉토리 복사 구현
    // mkdir() 사용하여 대상 디렉토리 생성
    // opendir()/readdir() 사용하여 재귀적으로 내용물 복사
    return 0; // TODO: 구현 필요
}

int moveFile(const char *src, const char *dst) {
    // 같은 파일시스템 내에서는 rename() 사용
    if (rename(src, dst) == 0) {
        return 0;
    }
    
    // 다른 파일시스템일 경우: 복사 후 원본 삭제
    if (errno == EXDEV) {
        if (copyFile(src, dst) == 0) {
            return removeFile(src);
        }
    }
    
    return -1;
}

int removeFile(const char *path) {
    struct stat st;
    
    if (stat(path, &st) == -1) {
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        return rmdir(path);  // 빈 디렉토리만 삭제 가능
    } else {
        return unlink(path);
    }
}

int getPathInput(const char *prompt, char *result, size_t maxlen) {
    WINDOW *input_win;
    int ch;
    int pos = 0;

    // 화면 하단에 입력창 생성
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    input_win = newwin(3, cols, rows-5, 0);
    box(input_win, 0, 0);
    
    // 프롬프트 표시
    mvwprintw(input_win, 1, 2, "%s: ", prompt);
    wrefresh(input_win);

    // 입력 처리
    keypad(input_win, TRUE);
    echo();
    curs_set(1);  // 커서 보이기

    // 결과 버퍼 초기화
    memset(result, 0, maxlen);

    while (1) {
        ch = wgetch(input_win);
        
        if (ch == '\n') {
            break;  // 입력 완료
        }
        else if (ch == 27) {  // ESC
            pos = -1;
            break;  // 취소
        }
        else if (ch == KEY_BACKSPACE || ch == 127) {
            if (pos > 0) {
                pos--;
                result[pos] = '\0';
                mvwprintw(input_win, 1, 2 + strlen(prompt) + 2 + pos, " ");
                wmove(input_win, 1, 2 + strlen(prompt) + 2 + pos);
            }
        }
        else if (pos < maxlen - 1 && isprint(ch)) {
            result[pos++] = ch;
            result[pos] = '\0';
        }
        
        // 현재 입력 내용 표시
        mvwprintw(input_win, 1, 2 + strlen(prompt) + 2, "%-*s", (int)maxlen, result);
        wrefresh(input_win);
    }

    // 원래 상태로 복구
    noecho();
    curs_set(0);  // 커서 숨기기
    delwin(input_win);
    touchwin(stdscr);
    refresh();

    if (pos == -1) {
        memset(result, 0, maxlen);  // 취소된 경우 버퍼 초기화
        return -1;  // 취소됨
    }
    
    return 0;
}