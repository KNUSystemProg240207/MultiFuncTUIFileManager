#ifndef _CONFIG_H_INCLUDED_
#define _CONFIG_H_INCLUDED_

#include <limits.h>

// 프로그램 이름 (왼쪽 상단에 표시됨)
#define PROG_NAME "MultiFuncFileManager"
#define PROG_NAME_LEN (sizeof(PROG_NAME) - 1)

// Delay들
#define FRAME_INTERVAL_USEC (50 * 1000)  // Framerate 제한 (단위: μs)
#define FRAME_PER_SECOND ((1000 * 1000) / FRAME_INTERVAL_USEC)  // 1초당 프레임 수
#define DIR_INTERVAL_USEC (1 * 1000 * 1000)  // 폴더 정보 새로고침 간격 (단위: μs)

#define MAX_DIRWINS 3  // 최대 가능한 '탭' 수
#define MAX_DIR_ENTRIES 1000  // 한 폴더에 표시 가능한 최대 Item 수
#ifndef NAME_MAX
#define NAME_MAX 255  // 표시할 최대 이름 길이 (Limit보다 더 길면: 잘림)
#endif

#define MAX_DISPLAY_LEN 20
#define MAX_PROCESSES 128  // 한 번에 표시 가능한 최대 프로세스 수
#ifndef PATH_MAX
#define PATH_MAX 4096  // 폴더의 최대 경로 길이
#endif
#define COPY_FILE_BUF_SIZE (4 * 1024)  // 4KB; copy_file_range 사용 불가한 경우, read() -> write()로 fallback됨

#define MAX_FILE_OPERATORS 4

// 날짜 및 시간 출력 형식
#define DATETIME_FORMAT "%2.2d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d"  // 날짜-시간 문자열 형식 (printf 형식)
#define DATETIME_LEN 19  // 날짜-시간 문자열 길이

#define MAX_BOTTOMBOX_MSG_LEN 64  // 하단 영역 메시지의 최대 길이 (화면 Size와 상관 없이)
#define MAX_POPUP_TITLE_LEN 64  // 하단 영역 메시지의 최대 길이 (화면 Size와 상관 없이)

#define MAX_SELECTION_TITLE_LEN 32  // 선택 팝업 창 제목 최대 길이
#define MAX_SELECTIONS 3  // 선택 팝업 창 최대 선택 개수
#define MAX_SELECTION_LEN 16  // 선택 팝업 창 항목 1개당 최대 문자열 길이

// struct dt -> 문자열 변환: 형식 지정 (buf 최소 길이: DATETIME_LEN + 1)
#define DT_TO_STR(buf, dt) sprintf((buf), DATETIME_FORMAT, (dt)->tm_year + 1900, (dt)->tm_mon + 1, (dt)->tm_mday, (dt)->tm_hour, (dt)->tm_min, (dt)->tm_sec)

#endif
