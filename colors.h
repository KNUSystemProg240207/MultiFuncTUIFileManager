#ifndef __COLORS_H_INCLUDED__
#define __COLORS_H_INCLUDED__
#include <ncurses.h>

typedef enum {
    NONE__,
    DIRECTORY,
    SYMBOLIC,
    EXE,
    IMG,
    HIDDEN_FOLDER,
    HIDDEN,
    BGRND,
    HEADER,
    DEFAULT
} FILE_COLOR;

#define COLOR_ORANGE 1
#define COLOR_NAVY 2
#define COLOR_DARKGRAY 3
#define COLOR_LESS_WHITE 4
#define COLOR_HOT_PINK 5
#define COLOR_GREEN_BLUE 6
#define COLOR_ALL_BLUE 8

void init_colorSet();


#endif
