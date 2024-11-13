#include "colors.h"
#include <stdlib.h>
#include "commons.h"
void init_colorSet() {
    start_color();
    if (has_colors() == FALSE) {
        endwin();
        exit(-1);
    }
    CHECK_FALSE1(has_colors(), "your terminal does not support Colors\n", -1);
    bkgd(COLOR_PAIR(HEADER));  // 배경 전체를 해당 색깔로 만듦

    init_pair(1, COLOR_WHITE, COLOR_BLACK);  // 기본: 하얀색 글자
    init_pair(2, COLOR_BLACK, COLOR_WHITE);  // 역상: 검은색 글자, 하얀색 배경
    init_pair(3, COLOR_BLUE, COLOR_BLACK);  // 디렉토리: 파란색 글자
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);  // 심볼릭 링크: 마젠타 글자
    init_pair(5, COLOR_GREEN, COLOR_WHITE);  // 실행 파일: 초록색 글자
    init_pair(6, COLOR_CYAN, COLOR_BLACK);  // 텍스트파일: 싸이안 글자
    init_pair(7, COLOR_RED, COLOR_BLACK);  // 이미지파일: 빨간색 글자
    init_pair(8, COLOR_YELLOW, COLOR_BLACK);  // 숨김파일: 노란색 글자
    init_pair(9, COLOR_WHITE, COLOR_BLUE);  // 파일헤더
}