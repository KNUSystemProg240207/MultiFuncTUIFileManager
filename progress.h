#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdint.h>
#include <ncurses.h>
#include "config.h"
#include "file_operator.h"

// flag에서 진행률 읽어오는 함수
int CalculProgress(FileProgressInfo info);

// 진행 상황을 bottomBox에 표시하는 함수
void displayProgress(WINDOW *bottomBox, int startY);

#endif  // PROGRESS_H