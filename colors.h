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
    POPUP,
    PAIR_LAST
} PAIR_COLOR;

// 전역 변수로 색상 초기화 성공 여부 관리
extern bool isColorSafe;

// 사용자 정의 색상
#define COLOR_ORANGE 16  // 주황색
#define COLOR_NAVY 17  // 네이비 색상
#define COLOR_DARKGRAY 18  // 어두운 회색
#define COLOR_LESS_WHITE 19  // 덜 밝은 흰색
#define COLOR_HOT_PINK 20  // 핫핑크
#define COLOR_GREEN_BLUE 21  // 초록빛 파랑색
#define COLOR_ALL_BLUE 22  // 전체 파란색
#define COLOR_DEEP_RED 23  // 다홍색
#define COLOR_LIGHT_GREEN 24  // 다홍색
#define COLOR_LAST 25  // 마지막 명시용 색깔(사용x)


/**
 * 색상을 초기화하고, 전역 변수 isColorSafe를 설정
 * - 사용자 정의 색상을 정의하고 색상 페어를 설정
 */
void initColors(void);

/**
 * 창에 색상을 적용
 *
 * @param win 색상을 적용할 창(WINDOW 포인터)
 * @param colorPair 적용할 색상 페어 번호
 */
void applyColor(WINDOW *win, int colorPair);

/**
 * 창에서 색상을 해제
 *
 * @param win 색상을 해제할 창(WINDOW 포인터)
 * @param colorPair 해제할 색상 페어 번호
 */
void removeColor(WINDOW *win, int colorPair);

#endif
