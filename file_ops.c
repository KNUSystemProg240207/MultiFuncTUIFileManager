#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "file_ops.h"

#define COPY_BUFFER_SIZE 8192

ssize_t copyFileOperation(FileTask *task, off_t offset, size_t size) {
    char buffer[COPY_BUFFER_SIZE];
    ssize_t totalCopied = 0;
    size_t remainingSize = size;
    
    // 소스 파일 열기
    int srcFd = openat(task->srcDirFd, task->srcName, O_RDONLY);
    if (srcFd == -1) return -1;

    // 목적지 파일 열기 (없으면 생성)
    int dstFd = openat(task->dstDirFd, task->dstName, 
                      O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        close(srcFd);
        return -1;
    }

    // 시작 위치로 이동
    if (lseek(srcFd, offset, SEEK_SET) == -1) {
        close(srcFd);
        close(dstFd);
        return -1;
    }

    // 지정된 크기만큼 복사
    while (remainingSize > 0) {
        size_t toRead = (remainingSize < COPY_BUFFER_SIZE) ? 
                        remainingSize : COPY_BUFFER_SIZE;
        
        ssize_t bytesRead = read(srcFd, buffer, toRead);
        if (bytesRead <= 0) break;

        ssize_t bytesWritten = write(dstFd, buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            totalCopied = -1;
            break;
        }

        totalCopied += bytesWritten;
        remainingSize -= bytesWritten;
    }

    close(srcFd);
    close(dstFd);
    return totalCopied;
}

int moveFileOperation(FileTask *task) {
    // 같은 디바이스인 경우 rename 사용
    if (task->srcDevNo == task->dstDevNo) {
        if (renameat(task->srcDirFd, task->srcName,
                    task->dstDirFd, task->dstName) == 0) {
            return 0;
        }
        if (errno != EXDEV) return -1;
    }

    // 다른 디바이스인 경우 복사 후 원본 삭제
    FileTask copyTask = *task;
    ssize_t copied = copyFileOperation(&copyTask, 0, task->fileSize);
    if (copied != task->fileSize) return -1;

    // 원본 파일 삭제
    if (unlinkat(task->srcDirFd, task->srcName, 0) != 0) {
        // 복사는 성공했지만 원본 삭제 실패
        unlinkat(task->dstDirFd, task->dstName, 0);  // 복사본 삭제 시도
        return -1;
    }

    return 0;
}

int deleteFileOperation(FileTask *task) {
    return unlinkat(task->srcDirFd, task->srcName, 0);
}

int executeFileOperation(FileOperation op) {
    char src_path[MAX_PATH_LEN];
    char dst_path[MAX_PATH_LEN];
    const char *selected_file = getCurrentSelection();
    
    if (!selected_file) return -1;
    
    // 소스 경로 생성
    snprintf(src_path, MAX_PATH_LEN, "%s/%s", current_path, selected_file);
    
    switch(op) {
        case COPY:
        case MOVE:
            // 임시로 같은 디렉토리에 "_copy" 붙여서 복사/이동
            snprintf(dst_path, MAX_PATH_LEN, "%s/%s_copy", 
                    current_path, selected_file);
            break;
        case DELETE:
            // 삭제는 dst_path 불필요
            break;
    }
    
    const char *selected_file = getCurrentSelection();
    if (!selected_file) return -1;
    
    FileTask task;
    memset(&task, 0, sizeof(FileTask));
    
    // 현재 디렉토리 파일 디스크립터 얻기
    task.srcDirFd = open(".", O_DIRECTORY);
    if (task.srcDirFd == -1) return -1;
    
    // 소스 파일 정보 설정
    strncpy(task.srcName, selected_file, MAX_NAME_LEN-1);
    
    // 파일 크기 얻기
    struct stat st;
    if (fstatat(task.srcDirFd, task.srcName, &st, 0) == 0) {
        task.fileSize = st.st_size;
        task.srcDevNo = st.st_dev;
    }
    
    int result = -1;
    switch(op) {
        case COPY:
        case MOVE:
            // TODO: UI에서 대상 경로 입력받기
            task.dstDirFd = task.srcDirFd;  // 임시로 같은 디렉토리
            snprintf(task.dstName, MAX_NAME_LEN, "%s_copy", selected_file);
            
            if (op == COPY) {
                result = (copyFileOperation(&task, 0, task.fileSize) == task.fileSize) ? 0 : -1;
            } else {
                result = moveFileOperation(&task);
            }
            break;
            
        case DELETE:
            result = deleteFileOperation(&task);
            break;
    }
    
    close(task.srcDirFd);
    if (task.dstDirFd >= 0 && task.dstDirFd != task.srcDirFd) {
        close(task.dstDirFd);
    }
    
    return result;
}