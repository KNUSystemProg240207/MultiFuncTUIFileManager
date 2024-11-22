#include <curses.h>

#include "input_window.h"

static WINDOW* inputWin;

char* getDestinationPath(const char* prompt) {
    static char path[MAX_NAME_LEN];

    // 입력 창 생성
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    inputWin = newwin(3, cols, rows - 5, 0);
    box(inputWin, 0, 0);

    // 프롬프트 표시
    mvwprintw(inputWin, 1, 2, "%s: ", prompt);
    wrefresh(inputWin);

    // 입력 모드 설정
    echo();
    curs_set(1);

    // 입력 받기
    wgetnstr(inputWin, path, MAX_NAME_LEN);

    // 원래 모드로 복구
    noecho();
    curs_set(0);

    // 창 제거
    delwin(inputWin);

    return path;
}