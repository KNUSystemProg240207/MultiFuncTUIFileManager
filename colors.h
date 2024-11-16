#ifndef __COLORS_H_INCLUDED__
#define __COLORS_H_INCLUDED__
#include <ncurses.h>

typedef enum {
    NONE__,
    BASIC,
    REVERSE_BASIC,
    DIRECTORY,
    SYMBOLIC,
    EXE,
    TXT,
    IMG,
    SRC,
    HIDDEN,
    BGRND,
    HEADER,
    OTHER,
    DEFAULT,
    SELECT
} FILE_COLOR;

#define COLOR_SKY 1
#define COLOR_ORANGE 2
void init_colorSet();


#endif
