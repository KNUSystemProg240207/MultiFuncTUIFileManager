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
    PRCSBGRND
} FILE_COLOR;

#define COLOR_ORANGE 1  // 주황색
#define COLOR_NAVY 2  // 네이비 색상
#define COLOR_DARKGRAY 3  // 어두운 회색
#define COLOR_LESS_WHITE 4  // 덜 밝은 흰색
#define COLOR_HOT_PINK 5  // 핫핑크
#define COLOR_GREEN_BLUE 6  // 초록빛 파랑색
// COLOR ID 7은 의도적으로 건너뛴 상태
#define COLOR_ALL_BLUE 8  // 전체 파란색
#define COLOR_DEEP_RED 9  // 다홍색

void calc255Color(short color_name, short r, short g, short b);
void initColorSet();


#endif
