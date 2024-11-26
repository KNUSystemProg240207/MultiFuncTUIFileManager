#ifndef __COLORS_H_INCLUDED__
#define __COLORS_H_INCLUDED__
#include <ncurses.h>

typedef enum {
    NONE__,
    DIRECTORY,
    SYMBOLIC,
    EXE,
    IMG,
    HIDDEN_FOLDER,
    HIDDEN,
    BGRND,
    HEADER,
    DEFAULT,
    PRCSBGRND,
    PRCSFILE,
    PAIR_LAST
} PAIR_COLOR;

// 전역 변수로 색상 초기화 성공 여부 관리
extern bool isColorSafe;

/* 사용자 정의 색상 */
#define COLOR_ORANGE 16  // 주황색
#define COLOR_NAVY 17  // 네이비 색상
#define COLOR_DARKGRAY 18  // 어두운 회색
#define COLOR_LESS_WHITE 19  // 덜 밝은 흰색
#define COLOR_HOT_PINK 20  // 핫핑크
#define COLOR_GREEN_BLUE 21  // 초록빛 파랑색
#define COLOR_ALL_BLUE 22  // 전체 파란색
#define COLOR_DEEP_RED 23  // 다홍색
#define COLOR_LAST 24  // 마지막 명시용 색깔(사용x)

/**
 * 사용자 정의 색상을 RGB 값으로 초기화합니다.
 *
 * @param color_name 정의할 색상의 이름 (숫자 코드).
 * @param r 빨강 값 (0 ~ 255).
 * @param g 초록 값 (0 ~ 255).
 * @param b 파랑 값 (0 ~ 255).
 */
void calc255Color(short color_name, short r, short g, short b);

/**
 * 색상을 초기화하고, 전역 변수 isColorSafe를 설정합니다.
 * - 사용자 정의 색상을 정의하고 색상 페어를 설정합니다.
 */
void initColorSet();

/**
 * 사용자 정의 색상 페어를 모두 초기화합니다.
 *
 * @return 모든 페어가 성공적으로 초기화되면 true, 실패하면 false.
 */
bool setColorPair();

/**
 * 특정 색상 페어가 안전하게 초기화되는지 확인합니다.
 *
 * @param pair_number 색상 페어 번호.
 * @param fg 전경색(foreground) 코드.
 * @param bg 배경색(background) 코드.
 * @return 초기화가 성공하면 true, 실패하면 false.
 */
bool isSafePair(short pair_number, short fg, short bg);

/**
 * 창에 색상을 적용합니다.
 *
 * @param win 색상을 적용할 창(WINDOW 포인터).
 * @param colorPair 적용할 색상 페어 번호.
 */
void applyColor(WINDOW *win, int colorPair);

/**
 * 창에서 색상을 해제합니다.
 *
 * @param win 색상을 해제할 창(WINDOW 포인터).
 * @param colorPair 해제할 색상 페어 번호.
 */
void removeColor(WINDOW *win, int colorPair);

#endif
