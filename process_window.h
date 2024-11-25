#ifndef PROC_WIN_H
#define PROC_WIN_H

#include <curses.h>
#include <panel.h>

#include "config.h"
#include "list_process.h"


int initProcessWindow(pthread_mutex_t *_bufMutex, size_t *_totalReadItems, Process *_processes);
void hideProcessWindow();
void delProcessWindow();
void updateProcessWindow();


/**
 * 프로세스 창의 테이블 헤더를 출력합니다.
 *
 * @param win 출력할 창(WINDOW 포인터).
 * @param winW 창의 너비.
 */
void printTableHeader(WINDOW *win, int winW);

/**
 * 프로세스 정보를 출력합니다.
 *
 * @param win 출력할 창(WINDOW 포인터).
 * @param winW 창의 너비.
 * @param processes 출력할 프로세스 배열.
 * @param maxItems 출력할 최대 항목 수.
 * @param totalItems 총 항목 수.
 */
void printProcessInfo(WINDOW *win, int winW, Process *processes, int maxItems, int totalItems);


/**
 * @brief 메모리 크기를 사람이 읽기 쉬운 형식(10KB, 1GB 등)으로 변환
 * @param size 바이트 단위의 메모리 크기
 * @return 사람이 읽기 쉬운 문자열 포맷 크기
 */
const char *formatSize(unsigned long size);

/**
 * @brief 클럭 틱(tick)을 초(second)로 변환
 * @param ticks 클럭 틱 값
 * @return 변환된 초 단위 시간
 */
unsigned long ticksToSeconds(unsigned long ticks);

#endif
