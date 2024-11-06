#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "commons.h"
#include "bottom_area.h"


static WINDOW *bottomBox;


WINDOW *initBottomBox(int width, int startY) {
    CHECK_NULL(bottomBox = newwin(2, width, startY, 0));
    return bottomBox;
}
