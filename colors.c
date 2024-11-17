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
    // bkgd(COLOR_PAIR(BGRND));  // 배경 전체를 해당 색깔로 만듦, 아직 확정x

    calcul255_color(COLOR_SKY, 143, 217, 239);
    calcul255_color(COLOR_ORANGE, 227, 142, 60);
    calcul255_color(COLOR_NAVY, 0x00, 0x1F, 0x3F);
    calcul255_color(COLOR_DARKBROWN, 0x4D, 0x2C, 0x2C);
    calcul255_color(COLOR_OLIVEGREEN, 49, 87, 22);


    init_pair(BASIC, COLOR_WHITE, COLOR_BLACK);  // 흰검 기본
    init_pair(REVERSE_BASIC, COLOR_BLACK, COLOR_WHITE);  // 흰검 기본 역상

    init_pair(DIRECTORY, COLOR_WHITE, COLOR_ORANGE);  // 디렉토리: 파란색
    init_pair(SYMBOLIC, COLOR_MAGENTA, COLOR_ORANGE);  // 심볼릭 링크: 마젠타
    init_pair(EXE, COLOR_GREEN, COLOR_ORANGE);  // 실행 파일: 초록색
    init_pair(SRC, COLOR_GREEN, COLOR_ORANGE);  // 실행 파일: 초록색
    init_pair(OTHER, COLOR_GREEN, COLOR_ORANGE);  // 실행 파일: 초록색
    init_pair(TXT, COLOR_CYAN, COLOR_ORANGE);  // 텍스트파일: 싸이안
    init_pair(IMG, COLOR_OLIVEGREEN, COLOR_ORANGE);  // 이미지파일: 빨간색
    init_pair(HIDDEN, COLOR_YELLOW, COLOR_ORANGE);  // 숨김파일: 노란색
    init_pair(HEADER, COLOR_WHITE, COLOR_ORANGE);  // 해더
    init_pair(BGRND, COLOR_WHITE, COLOR_ORANGE);  // 배경
    init_pair(DEFAULT, COLOR_DARKBROWN, COLOR_ORANGE);  // 기본 출력
    init_pair(SELECT, COLOR_WHITE, COLOR_NAVY);
}
