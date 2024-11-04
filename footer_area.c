#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "commons.h"
#include "footer_area.h"


WINDOW *initBottomBox(int width, int startY) {
    WINDOW *bottomBox = newwin(2, width, startY, 0);
    CHECK_NULL(bottomBox);
    CHECK_CURSES(init_pair(COLOR_PAIR_BOTTOM, COLOR_BOTTOM_FG, COLOR_BOTTOM_BG));
    // CHECK_CURSES(wattron(bottomBox, COLOR_PAIR(COLOR_PAIR_BOTTOM)));
    // CHECK_CURSES(mvwhline(bottomBox, 0, 0, ' ', width));
    // CHECK_CURSES(mvwhline(bottomBox, 1, 0, ' ', width));
    return bottomBox;
}
