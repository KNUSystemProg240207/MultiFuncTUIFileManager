#include <stdlib.h>

#include "colors.h"
#include "commons.h"

void calcul255_color(short color_name, short r, short g, short b) {
    r *= (1000 / 255);
    g *= (1000 / 255);
    b *= (1000 / 255);
    init_color(color_name, r, g, b);
}
void init_colorSet() {
    calcul255_color(COLOR_ORANGE, 250, 182, 87);
    calcul255_color(COLOR_NAVY, 0x00, 0x1F, 0x3F);
    calcul255_color(COLOR_DARKGRAY, 115, 115, 115);
    calcul255_color(COLOR_LESS_WHITE, 255, 255, 255);
    calcul255_color(COLOR_HOT_PINK, 255, 46, 123);
    calcul255_color(COLOR_GREEN_BLUE, 0x00, 0xB3, 0xB3);
    calcul255_color(COLOR_ALL_BLUE, 0x00, 0x00, 0xFF);

    init_pair(DIRECTORY, COLOR_WHITE, COLOR_ORANGE);  // 디렉토리: 하얀색
    init_pair(SYMBOLIC, COLOR_HOT_PINK, COLOR_ORANGE);  // 심볼릭 링크: 핫핑크
    init_pair(EXE, COLOR_ALL_BLUE, COLOR_ORANGE);  // 실행파일 : 파란색
    init_pair(IMG, COLOR_GREEN_BLUE, COLOR_ORANGE);  // 이미지파일: 청록색
    init_pair(HIDDEN_FOLDER, COLOR_LESS_WHITE, COLOR_ORANGE);  // 숨김폴더: 뿌연 흰색
    init_pair(HIDDEN, COLOR_DARKGRAY, COLOR_ORANGE);  // 숨김파일: 회색
    init_pair(HEADER, COLOR_WHITE, COLOR_ORANGE);  // 헤더 : 흰색
    init_pair(BGRND, COLOR_WHITE, COLOR_ORANGE);  // 배경(박스) : 흰색
    init_pair(DEFAULT, COLOR_NAVY, COLOR_ORANGE);  // 기본 파일 : 네이비
}
