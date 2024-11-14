#ifndef __COLORS_H_INCLUDED__
#define __COLORS_H_INCLUDED__
#include <ncurses.h>

typedef enum {
    NONE__,
    DEFAULT,
    REVERSE_NORMAL,
    DIRECTORY,
    SYMBOLIC,
    EXE,
    TXT,
    IMG,
    HIDDEN,
    HEADER
} FILE_COLOR;

void init_colorSet();


#endif
