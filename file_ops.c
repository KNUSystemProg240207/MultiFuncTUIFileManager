#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "file_ops.h"

#define BUFFER_SIZE 8192

static int copyFileContents(int srcFd, int dstFd, size_t fileSize) {
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead, bytesWritten;
    size_t remainingBytes = fileSize;

    while (remainingBytes > 0) {
        // 버퍼 크기와 남은 바이트 중 작은 값으로 읽기
        bytesRead = read(srcFd, buffer, 
            remainingBytes < BUFFER_SIZE ? remainingBytes : BUFFER_SIZE);
        
        if (bytesRead <= 0) {
            return -1;
        }

        // 읽은 데이터 쓰기
        bytesWritten = write(dstFd, buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            return -1;
        }

        remainingBytes -= bytesRead;
    }

    return 0;
}

int copyFile(FileTask *task) {
    struct stat srcStat;
    int srcFd, dstFd;
    int result = -1;

    // 소스 파일 열기
    srcFd = openat(task->srcDirFd, task->srcName, O_RDONLY);
    if (srcFd == -1) {
        return -1;
    }

    // 소스 파일의 권한 정보 가져오기
    if (fstat(srcFd, &srcStat) == -1) {
        close(srcFd);
        return -1;
    }

    // 대상 파일 생성
    dstFd = openat(task->dstDirFd, task->dstName, 
        O_WRONLY | O_CREAT | O_TRUNC, srcStat.st_mode);
    if (dstFd == -1) {
        close(srcFd);
        return -1;
    }

    // 파일 내용 복사
    result = copyFileContents(srcFd, dstFd, task->fileSize);

    // 파일 디스크립터 정리
    close(srcFd);
    close(dstFd);

    return result;
}

int moveFile(FileTask *task) {
    // 같은 디바이스인 경우: rename 시도
    if (task->srcDevNo == task->dstDevNo) {
        if (renameat(task->srcDirFd, task->srcName, 
            task->dstDirFd, task->dstName) == 0) {
            return 0;
        }
        // rename 실패하면 복사+삭제로 진행
    }

    // 다른 디바이스이거나 rename 실패한 경우: 복사 후 원본 삭제
    if (copyFile(task) == -1) {
        return -1;
    }

    // 원본 파일 삭제
    if (unlinkat(task->srcDirFd, task->srcName, 0) == -1) {
        // 원본 삭제 실패 시 복사본도 삭제
        unlinkat(task->dstDirFd, task->dstName, 0);
        return -1;
    }

    return 0;
}

int deleteFile(FileTask *task) {
    return unlinkat(task->srcDirFd, task->srcName, 0);
}



static FileTask currentTask;

int initFileOperation(FileOperation type) {
    currentTask.type = type;
    
    // 현재 선택된 파일 정보 가져오기
    const char* srcName = getCurrentFileName();
    if (!srcName) return -1;
    
    strncpy(currentTask.srcName, srcName, MAX_NAME_LEN);
    currentTask.fileSize = getCurrentFileSize();
    
    // 현재 디렉토리 파일 디스크립터 얻기
    currentTask.srcDirFd = open(".", O_DIRECTORY);
    if (currentTask.srcDirFd == -1) return -1;
    
    // 소스 디바이스 번호 얻기
    struct stat st;
    if (stat(".", &st) == -1) {
        close(currentTask.srcDirFd);
        return -1;
    }
    currentTask.srcDevNo = st.st_dev;
    
    // 작업 타입에 따른 처리
    switch (type) {
        case COPY:
        case MOVE:
            // TODO: 대상 경로 입력 받기
            break;
        case DELETE:
            return deleteFile(&currentTask);
    }
    
    return 0;
}

int executeFileOperation(void) {
    switch (currentTask.type) {
        case COPY:
            return copyFile(&currentTask);
        case MOVE:
            return moveFile(&currentTask);
        case DELETE:
            return deleteFile(&currentTask);
        default:
            return -1;
    }
}