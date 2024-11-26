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
    init_color(color_name, r, g, b);
    return true;
}

void initColorSet() {
    // 지원하는 색깔이 적을 경우, 기본 색으로 출력
    if (COLORS < COLOR_LAST || COLOR_PAIRS < PAIR_LAST) {
        for (int i = NONE__; i <= PAIR_LAST; i++) {
            init_pair(i, COLOR_WHITE, COLOR_BLACK);
        }
        isColorSafe = false;
        return;
    }

    // 색상 정의
    if (!calc255Color(COLOR_ORANGE, 250, 182, 87)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_NAVY, 0x00, 0x1F, 0x3F)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_DARKGRAY, 115, 115, 115)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_LESS_WHITE, 255, 255, 255)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_HOT_PINK, 255, 46, 123)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_GREEN_BLUE, 0x00, 0xB3, 0xB3)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_ALL_BLUE, 0x00, 0x00, 0xFF)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_DEEP_RED, 250, 155, 87)) {
        isColorSafe = false;
        return;
    }
    if (!calc255Color(COLOR_LIGHT_GREEN, 227, 255, 87)) {
        isColorSafe = false;
        return;
    }

    // 색상 페어 초기화
    if (init_pair(DIRECTORY, COLOR_WHITE, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 디렉토리
        return;
    }
    if (init_pair(SYMBOLIC, COLOR_HOT_PINK, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 심볼릭 링크
        return;
    }
    if (init_pair(EXE, COLOR_ALL_BLUE, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 실행파일
        return;
    }
    if (init_pair(IMG, COLOR_GREEN_BLUE, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 이미지파일
        return;
    }
    if (init_pair(HIDDEN_FOLDER, COLOR_LESS_WHITE, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 숨김폴더
        return;
    }
    if (init_pair(HIDDEN, COLOR_DARKGRAY, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 숨김파일
        return;
    }
    if (init_pair(HEADER, COLOR_WHITE, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 헤더
        return;
    }
    if (init_pair(BGRND, COLOR_WHITE, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 배경(박스)
        return;
    }
    if (init_pair(DEFAULT, COLOR_NAVY, COLOR_ORANGE) == ERR) {
        isColorSafe = false;  // 기본 파일
        return;
    }
    if (init_pair(PRCSBGRND, COLOR_WHITE, COLOR_DEEP_RED) == ERR) {
        isColorSafe = false;  // 프로세스 창 배경
        return;
    }
    if (init_pair(PRCSFILE, COLOR_BLACK, COLOR_DEEP_RED) == ERR) {
        isColorSafe = false;  // 숨김파일
        return;
    }
    if (init_pair(POPUP, COLOR_WHITE, COLOR_LIGHT_GREEN) == ERR) {
        isColorSafe = false;  // 팝업
        return;
    }
    isColorSafe = true;  // 모든 페어가 성공적으로 초기화됨
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
