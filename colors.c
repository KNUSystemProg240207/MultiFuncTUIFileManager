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

    init_pair(BASIC, COLOR_WHITE, COLOR_BLACK);  // 흰검 기본
    init_pair(REVERSE_BASIC, COLOR_BLACK, COLOR_WHITE);  // 흰검 기본 역상

    init_pair(DIRECTORY, COLOR_WHITE, COLOR_BLUE);  // 디렉토리: 파란색
    init_pair(SYMBOLIC, COLOR_MAGENTA, COLOR_BLUE);  // 심볼릭 링크: 마젠타
    init_pair(EXE, COLOR_GREEN, COLOR_BLUE);  // 실행 파일: 초록색
    init_pair(SRC, COLOR_GREEN, COLOR_BLUE);  // 실행 파일: 초록색
    init_pair(OTHER, COLOR_GREEN, COLOR_BLUE);  // 실행 파일: 초록색
    init_pair(TXT, COLOR_CYAN, COLOR_BLUE);  // 텍스트파일: 싸이안
    init_pair(IMG, COLOR_RED, COLOR_BLUE);  // 이미지파일: 빨간색
    init_pair(HIDDEN, COLOR_YELLOW, COLOR_BLUE);  // 숨김파일: 노란색
    init_pair(HEADER, COLOR_YELLOW, COLOR_BLUE);  // 해더
    init_pair(BGRND, COLOR_SKY, COLOR_BLUE);  // 배경
    init_pair(DEFAULT, COLOR_SKY, COLOR_BLUE);  // 기본 출력
    init_pair(SELECT, COLOR_WHITE, COLOR_SKY);
}
