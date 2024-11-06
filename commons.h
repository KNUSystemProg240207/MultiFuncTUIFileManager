#ifndef __COMMONS_H_INCLUDED__
#define __COMMONS_H_INCLUDED__

#include <curses.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// '오류 확인 후 종료' 관련 매크로 함수들

#define CHECK_CURSES1(ret, msg, errno) do {\
    if ((ret) == ERR) {\
        fprintf(stderr, msg);\
        exit(errno);\
    }\
} while (0)
#define CHECK_CURSES(ret) CHECK_CURSES1(ret, "Error", -1)

#define CHECK_NULL1(ret, msg, errno) do {\
    if ((ret) == NULL) {\
        fprintf(stderr, msg);\
        exit(errno);\
    }\
} while (0)
#define CHECK_NULL(ret) CHECK_NULL1(ret, "Error", -1)

#define CHECK_FALSE1(ret, msg, errno) do {\
    if ((ret) == FALSE) {\
        fprintf(stderr, msg);\
        exit(errno);\
    }\
} while (0)
#define CHECK_FALSE(ret) CHECK_FALSE1(ret, "Error", -1)

#define CHECK_FAIL1(ret, msg, errno) do {\
    if ((ret) == -1) {\
        perror(msg);\
        exit(errno);\
    }\
} while (0)
#define CHECK_FAIL(ret) CHECK_FAIL1(ret, "Error", -1)


/**
 * (현재 시간) - (시작 시간) 계산해서 돌려줌
 * 
 * @param baseTime 시작 시간 (Clock: CLOCK_MONOTONIC 사용)
 * @return 흐른 시간 (단위: μs)
 */
uint64_t getElapsedTime(struct timespec baseTime);

#endif
