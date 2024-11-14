//진행상태 모니터링 관리 헤더

typedef struct {
    size_t total_size;
    size_t processed_size;
    char current_file[MAX_PATH_LEN];
    int percent_complete;
} ProgressInfo;

// 진행 상태 업데이트
void updateProgress(ProgressInfo *info, size_t bytes_processed);

// 진행 상태 표시
void displayProgress(UIContext *ui, ProgressInfo *info);