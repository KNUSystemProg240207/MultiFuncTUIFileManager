#include <stdbool.h>
#include <stdlib.h>

#include "colors.h"
#include "commons.h"

bool isColorSafe = false;

/**
 * 사용자 정의 색상을 RGB 값으로 초기화
 *
 * @param color_name 정의할 색상의 이름 (숫자 코드)
 * @param r 빨강 값 (0 ~ 255)
 * @param g 초록 값 (0 ~ 255)
 * @param b 파랑 값 (0 ~ 255)
 */
static bool calc255Color(short color_name, short r, short g, short b) {
    if (color_name >= COLORS) {
        fprintf(stderr, "Warning: Color ID %d exceeds maximum COLORS (%d).\n", color_name, COLORS);
        return false;
    }
    r *= (1000 / 255);
    g *= (1000 / 255);
    b *= (1000 / 255);
    if (init_color(color_name, r, g, b) == ERR)
        return false;
    return true;
}

void initColors() {
    // clang-format off
    // 지원하는 색깔이 적을 경우, 기본 색으로 출력
    if (
           has_colors() == FALSE
        || start_color() == ERR
        || can_change_color() == FALSE
        || COLORS < COLOR_LAST
        || COLOR_PAIRS < PAIR_LAST
        // 색상 정의
        || !calc255Color(COLOR_ORANGE, 250, 182, 87)
        || !calc255Color(COLOR_NAVY, 0x00, 0x1F, 0x3F)
        || !calc255Color(COLOR_DARKGRAY, 115, 115, 115)
        || !calc255Color(COLOR_LESS_WHITE, 255, 255, 255)
        || !calc255Color(COLOR_HOT_PINK, 255, 46, 123)
        || !calc255Color(COLOR_GREEN_BLUE, 0x00, 0xB3, 0xB3)
        || !calc255Color(COLOR_ALL_BLUE, 0x00, 0x00, 0xFF)
        || !calc255Color(COLOR_DEEP_RED, 250, 155, 87)
        || !calc255Color(COLOR_LIGHT_GREEN, 227, 255, 87)
        // 색 Pair 정의
        || init_pair(DIRECTORY, COLOR_WHITE, COLOR_ORANGE) == ERR  // 디렉토리
        || init_pair(SYMBOLIC, COLOR_HOT_PINK, COLOR_ORANGE) == ERR  // 심볼릭 링크
        || init_pair(EXE, COLOR_ALL_BLUE, COLOR_ORANGE) == ERR  // 실행파일
        || init_pair(IMG, COLOR_GREEN_BLUE, COLOR_ORANGE) == ERR  // 이미지파일
        || init_pair(HIDDEN_FOLDER, COLOR_LESS_WHITE, COLOR_ORANGE) == ERR  // 숨김폴더
        || init_pair(HIDDEN, COLOR_DARKGRAY, COLOR_ORANGE) == ERR  // 숨김파일
        || init_pair(HEADER, COLOR_WHITE, COLOR_ORANGE) == ERR  // 헤더
        || init_pair(BGRND, COLOR_WHITE, COLOR_ORANGE) == ERR  // 배경(박스)
        || init_pair(DEFAULT, COLOR_NAVY, COLOR_ORANGE) == ERR  // 기본 파일
        || init_pair(PRCSBGRND, COLOR_WHITE, COLOR_DEEP_RED) == ERR  // 프로세스 창 배경
        || init_pair(PRCSFILE, COLOR_BLACK, COLOR_DEEP_RED) == ERR  // 숨김파일
        || init_pair(POPUP, COLOR_WHITE, COLOR_LIGHT_GREEN) == ERR  // 팝업
    ) {  
        isColorSafe = false;
        return;
    }
    // clang-format on
    // 흰색을 '진짜' 흰색으로: 일부 환경에서, COLOR_WHITE가 회색인 경우 있음
    init_color(COLOR_WHITE, 1000, 1000, 1000);  // 필수 사항 X -> 실패해도 계속 진행
    isColorSafe = true;  // 색 지원, 재정의 가능, 색 재정의 모두 성공
}

// 색상 적용
void applyColor(WINDOW *win, int colorPair) {
    if (isColorSafe)
        wattron(win, COLOR_PAIR(colorPair));
}

// 색상 해제
void removeColor(WINDOW *win, int colorPair) {
    if (isColorSafe)
        wattroff(win, COLOR_PAIR(colorPair));
}
