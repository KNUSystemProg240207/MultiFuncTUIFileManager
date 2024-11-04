#ifndef __TITLE_BAR_H_INCLUDED__
#define __TITLE_BAR_H_INCLUDED__

#include <curses.h>


WINDOW *initTitleBar(int width);
void renderTime();
void printPath(char *path);

#endif
