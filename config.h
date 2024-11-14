#ifndef __CONFIG_H_INCLUDED__
#define __CONFIG_H_INCLUDED__


// 프로그램 이름 (왼쪽 상단에 표시됨)
#define PROG_NAME "Demo"
#define PROG_NAME_LEN (sizeof(PROG_NAME) - 1)

// Delay들
#define FRAME_INTERVAL_USEC (50 * 1000)  // Framerate 제한 (단위: μs)
#define DIR_INTERVAL_USEC (1 * 1000 * 1000)  // 폴더 정보 새로고침 간격 (단위: μs)

#define MAX_DIRWINS 1  // 최대 가능한 '탭' 수
// #define MAX_DIRWINS 3
#define MAX_STAT_ENTRIES 100  // 한 폴더에 표시 가능한 최대 Item 수
#define MAX_PATH_LEN 4096  // 추가
#define MAX_NAME_LEN 255  // 표시할 최대 이름 길이 (Limit보다 더 길면: 잘림)
#define MAX_CWD_LEN 2048  // 최대 '현재 경로' 길이

// 날짜 및 시간 출력 형식
#define DATETIME_FORMAT "%2.2d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d"  // 날짜-시간 문자열 형식 (printf 형식)
#define DATETIME_LEN 19  // 날짜-시간 문자열 길이

// struct dt -> 문자열 변환: 형식 지정 (buf 최소 길이: DATETIME_LEN + 1)
#define DT_TO_STR(buf, dt) sprintf((buf), DATETIME_FORMAT, (dt)->tm_year + 1900, (dt)->tm_mon + 1, (dt)->tm_mday, (dt)->tm_hour, (dt)->tm_min, (dt)->tm_sec)

typedef struct {
    int confirm_delete;
    int show_hidden_files;
    int preserve_attributes;
    size_t buffer_size;
    char default_copy_dir[MAX_PATH_LEN];  // 이제 정의되어 있음 - 오류수정 완
} FileManagerConfig;

// 설정 로드/저장
int loadConfig(FileManagerConfig *config);
int saveConfig(FileManagerConfig *config);

#endif
