#include <stdlib.h>

#include "colors.h"
#include "commons.h"

bool isColorSafe = false;

void calc255Color(short color_name, short r, short g, short b) {
    if (color_name >= COLORS) {
        fprintf(stderr, "Warning: Color ID %d exceeds maximum COLORS (%d).\n", color_name, COLORS);
        return;
    }
    r *= (1000 / 255);
    g *= (1000 / 255);
    b *= (1000 / 255);
    init_color(color_name, r, g, b);
}

void initColorSet() {
    /* 지원하는 색깔이 적을 경우, 기본 색으로 출력 */
    if (COLORS < COLOR_LAST || COLOR_PAIRS < PAIR_LAST) {
        for (int i = NONE__; i <= PAIR_LAST; i++) {
            init_pair(i, COLOR_WHITE, COLOR_BLACK);  //
        }
        isColorSafe = false;
        return;
    }

    /* 색상 정의 */
    calc255Color(COLOR_ORANGE, 250, 182, 87);
    calc255Color(COLOR_NAVY, 0x00, 0x1F, 0x3F);
    calc255Color(COLOR_DARKGRAY, 115, 115, 115);
    calc255Color(COLOR_LESS_WHITE, 255, 255, 255);
    calc255Color(COLOR_HOT_PINK, 255, 46, 123);
    calc255Color(COLOR_GREEN_BLUE, 0x00, 0xB3, 0xB3);
    calc255Color(COLOR_ALL_BLUE, 0x00, 0x00, 0xFF);
    calc255Color(COLOR_DEEP_RED, 250, 155, 87);

    /* 색상 페어 초기화 */
    isColorSafe = setColorPair();
}

bool setColorPair() {
    // 색상 페어 초기화
    if (!isSafePair(DIRECTORY, COLOR_WHITE, COLOR_ORANGE)) return false;  // 디렉토리
    if (!isSafePair(SYMBOLIC, COLOR_HOT_PINK, COLOR_ORANGE)) return false;  // 심볼릭 링크
    if (!isSafePair(EXE, COLOR_ALL_BLUE, COLOR_ORANGE)) return false;  // 실행파일
    if (!isSafePair(IMG, COLOR_GREEN_BLUE, COLOR_ORANGE)) return false;  // 이미지파일
    if (!isSafePair(HIDDEN_FOLDER, COLOR_LESS_WHITE, COLOR_ORANGE)) return false;  // 숨김폴더
    if (!isSafePair(HIDDEN, COLOR_DARKGRAY, COLOR_ORANGE)) return false;  // 숨김파일
    if (!isSafePair(HEADER, COLOR_WHITE, COLOR_ORANGE)) return false;  // 헤더
    if (!isSafePair(BGRND, COLOR_WHITE, COLOR_ORANGE)) return false;  // 배경(박스)
    if (!isSafePair(DEFAULT, COLOR_NAVY, COLOR_ORANGE)) return false;  // 기본 파일
    if (!isSafePair(PRCSBGRND, COLOR_WHITE, COLOR_DEEP_RED)) return false;  // 프로세스 창 배경
    if (!isSafePair(PRCSFILE, COLOR_BLACK, COLOR_DEEP_RED)) return false;  // 숨김파일

    return true;  // 모든 페어가 성공적으로 초기화됨
}

/* init_pair 호출 시 실패를 체크하고 에러를 반환 */
bool isSafePair(short pair_number, short fg, short bg) {
    if (init_pair(pair_number, fg, bg) == ERR) {
        return false;  // 실패한 경우 false 반환
    }
    return true;  // 성공한 경우 true 반환
}

// 색상 적용
void applyColor(WINDOW *win, int colorPair) {
    if (isColorSafe) wattron(win, COLOR_PAIR(colorPair));
}

// 색상 해제
void removeColor(WINDOW *win, int colorPair) {
    if (isColorSafe) wattroff(win, COLOR_PAIR(colorPair));
}
