#ifndef SELECTION_WINDOW_H
#define SELECTION_WINDOW_H

#include "config.h"

// 외부에서 접근할 함수 선언
void initSelectionWindow(void);
void showSelectionWindow(const char *title, int selCnt, ...);
void hideSelectionWindow(void);
void delSelectionWindow(void);

void updateSelectionWindow(void);

void selectionWindowSelNext(void);
void selectionWindowSelPrevious(void);
int selectionWindowGetSel(void);

#endif
