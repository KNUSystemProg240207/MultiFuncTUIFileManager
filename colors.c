#include "colors.h"
#include <stdlib.h>
#include "commons.h"
void init_colorSet() {
    bkgd(COLOR_PAIR(HEADER));  // 배경 전체를 해당 색깔로 만듦, 아직 확정x

    init_pair(1, COLOR_WHITE, COLOR_BLACK);  // 기본
    init_pair(2, COLOR_BLACK, COLOR_WHITE);  // 기본 역상
    init_pair(3, COLOR_BLUE, COLOR_BLACK);  // 디렉토리: 파란색
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);  // 심볼릭 링크: 마젠타
    init_pair(5, COLOR_GREEN, COLOR_WHITE);  // 실행 파일: 초록색
    init_pair(6, COLOR_CYAN, COLOR_BLACK);  // 텍스트파일: 싸이안
    init_pair(7, COLOR_RED, COLOR_BLACK);  // 이미지파일: 빨간색
    init_pair(8, COLOR_YELLOW, COLOR_BLACK);  // 숨김파일: 노란색
    init_pair(9, COLOR_WHITE, COLOR_BLUE);  // 파일헤더
}