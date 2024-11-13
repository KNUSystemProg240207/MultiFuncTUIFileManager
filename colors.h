#ifndef __COLORS_H_INCLUDED__
#define __COLORS_H_INCLUDED__
#include <ncurses.h>

void init_colors();

typedef enum {
    NONE__,
    NORMAL,  // 1
    REVERSE_NORMAL,
    DIRECTORY,
    SYMBOLIC,
    EXE,
    TXT,
    IMG,  // 7

} FILE_COLOR;


#endif
