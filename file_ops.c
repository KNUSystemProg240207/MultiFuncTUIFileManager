#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#include "file_ops.h"

#define COPY_CHUNK_SIZE (1024 * 1024)  // 1MB 단위로 복사

#define SET_FILE_OPERATION_FLAG(progress, fileName, operationType) do {\
    pthread_mutex_lock((progress)->flagMutex);\
    *(progress)->flags = operationType;\
    strcpy((progress)->name, (fileName));\
    pthread_mutex_unlock((progress)->flagMutex);\
} while (0)
#define CLEAR_FILE_OPERATION_FLAG(progress) do {\
    pthread_mutex_lock((progress)->flagMutex);\
    *(progress)->flags |= ~PROGRESS_BITS;\
    pthread_mutex_unlock((progress)->flagMutex);\
} while (0)

/**
 * COPY_CHUNK_SIZE 단위로 분할 복사, 진행률 갱신
 *
 * @param srcFd 원본 파일 descriptor
 * @param dstFd 대상 파일 descriptor
 * @param fileSize 원본 파일 크기
 * @param progress 진행 상태 구조체
 * @param flagMutex 진행률 보호 Mutex
 * @return 성공: 0, 실패: -1
 */
static int doCopyFile(int srcFd, int dstFd, size_t fileSize, FileProgressInfo *progress) {
    off_t off_in = 0, off_out = 0;
    size_t totalCopied = 0, chunk;
    ssize_t copied;

    while (totalCopied < fileSize) {
        chunk = ((fileSize - totalCopied) < COPY_CHUNK_SIZE) ? (fileSize - totalCopied) : COPY_CHUNK_SIZE;
        copied = copy_file_range(srcFd, &off_in, dstFd, &off_out, chunk, 0);
        if (copied == -1) {
            return -1;
        }

        totalCopied += copied;

        // 진행률 업데이트
        pthread_mutex_lock(progress->flagMutex);
        *progress->flags &= ~PROGRESS_PERCENT_MASK;
        *progress->flags |= (totalCopied / fileSize * 100) << PROGRESS_PERCENT_START;
        pthread_mutex_unlock(progress->flagMutex);
    }

    return (totalCopied == fileSize) ? 0 : -1;
}

int copyFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress) {
    SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_COPY);

    int srcFd = openat(src->dirFd, src->name, O_RDONLY);
    if (srcFd == -1) {
        CLEAR_FILE_OPERATION_FLAG(progress);
        return -1;
    }
    int dstFd = openat(dst->dirFd, dst->name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        close(srcFd);
        CLEAR_FILE_OPERATION_FLAG(progress);
        return -1;
    }

    // 복사 수행
    int result = doCopyFile(srcFd, dstFd, src->fileSize, progress);

    close(srcFd);
    close(dstFd);

    if (result == -1) {
        unlinkat(dst->dirFd, dst->name, 0);
    }
    CLEAR_FILE_OPERATION_FLAG(progress);
    return result;
}

int moveFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress) {
    // 같은 디바이스인 경우, 진행률 표시 X
    // 다른 디바이스인 경우, 복사 함수를 통해 진행률 표시
    SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_MOVE);

    // 같은 디바이스면 rename
    if (src->devNo == dst->devNo && renameat(src->dirFd, src->name, dst->dirFd, dst->name) == 0) {
        CLEAR_FILE_OPERATION_FLAG(progress);
        return 0;
    }

    // 다른 디바이스면 복사 후 원본 삭제
    int srcFd = openat(src->dirFd, src->name, O_RDONLY);
    if (srcFd == -1) {
        CLEAR_FILE_OPERATION_FLAG(progress);
        return -1;
    }
    int dstFd = openat(dst->dirFd, dst->name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        CLEAR_FILE_OPERATION_FLAG(progress);
        close(srcFd);
        return -1;
    }

    // 복사 수행
    int result = doCopyFile(srcFd, dstFd, src->fileSize, progress);

    close(srcFd);
    close(dstFd);

    if (result == -1) {
        int ret = unlinkat(dst->dirFd, dst->name, 0);
        CLEAR_FILE_OPERATION_FLAG(progress);
        return ret;
    }
    int ret = unlinkat(src->dirFd, src->name, 0);
    CLEAR_FILE_OPERATION_FLAG(progress);
    return ret;
}

int removeFile(SrcDstInfo *src, FileProgressInfo *progress) {
    // 삭제: atomic 작업 -> 시작만 표시
    SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_DELETE);
    int ret = unlinkat(src->dirFd, src->name, 0);
    CLEAR_FILE_OPERATION_FLAG(progress);
    return ret;
}