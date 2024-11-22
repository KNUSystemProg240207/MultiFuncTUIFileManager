#include <curses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "bottom_area.h"
#include "commons.h"
#include "config.h"


static WINDOW *bottomBox;


WINDOW *initBottomBox(int width, int startY) {
    CHECK_NULL(bottomBox = newwin(2, width, startY, 0));
    return bottomBox;
}
