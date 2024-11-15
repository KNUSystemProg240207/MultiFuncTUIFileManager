#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdint.h>
#include <ncurses.h>
#include "config.h"

#define NONE 0
#define COPYING (1 << 7)
#define MOVING (1 << 8)
#define REMOVING (1 << 9)

#define PERCENTAGE_MASK 0x7F  // 진행률 마스크 (0-6 비트 사용, 7비트)
#define STATUS_MASK (COPYING | MOVING | REMOVING)  // 작업 상태 마스크

// 파일 작업의 이름 저장 배열 및 진행 상태 배열 선언
extern char currentFile[MAX_FILE_OPERATORS][MAX_NAME_LEN];
extern uint16_t progress[MAX_FILE_OPERATORS];

// 진행률 업데이트 함수
void updateProgress(uint16_t *prog, uint8_t percent);

// 진행률 가져오기 함수
uint8_t getPercentage(uint16_t prog);

// 파일 이름 설정 함수
void setFilename(int index, const char *fileName);

// 작업 상태 확인 함수들
int isCopying(uint16_t prog);
int isMoving(uint16_t prog);
int isDeleting(uint16_t prog);

// 오프셋을 이용한 진행률 계산 함수
void CalculProgress(int index, uint64_t offset, uint64_t total_size);

// 진행 상황을 bottomBox에 표시하는 함수
void displayProgress(WINDOW *bottomBox, int startY);

#endif  // PROGRESS_H