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
    return bottomBox;
}
