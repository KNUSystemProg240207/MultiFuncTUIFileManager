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

/**
 * COPY_CHUNK_SIZE 단위로 분할 복사, 진행률 갱신
 *
 * @param srcFd 원본 파일 descriptor
 * @param dstFd 대상 파일 descriptor
 * @param fileSize 원본 파일 크기
 * @param progress 진행 상태 구조체
 * @param progressMutex 진행률 보호 Mutex
 * @return 성공: 0, 실패: -1
 */
static int doCopyFile(int srcFd, int dstFd, size_t fileSize, FileProgressInfo *progress, pthread_mutex_t *progressMutex) {
    off_t off_in = 0, off_out = 0;
    size_t totalCopied = 0, chunk;
    ssize_t copied;

    while (totalCopied < fileSize) {
        chunk = ((fileSize - totalCopied) < COPY_CHUNK_SIZE) ? (fileSize - totalCopied) : COPY_CHUNK_SIZE;
        copied = copy_file_range(srcFd, &off_in, dstFd, &off_out, chunk, 0);
        if (copied == -1) {
            if (errno == EINVAL || errno == EXDEV) {
                return -1;
            }
            return -1;
        }

        totalCopied += copied;

        // 진행률 업데이트
        pthread_mutex_lock(progressMutex);
        *progress->flags &= ~PROGRESS_PERCENT_MASK;
        *progress->flags |= (totalCopied / fileSize * 100) << PROGRESS_PERCENT_START;
        pthread_mutex_unlock(progressMutex);
    }

    return (totalCopied == fileSize) ? 0 : -1;
}

int copyFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress, pthread_mutex_t *progressMutex) {
    int srcFd = openat(src->dirFd, src->name, O_RDONLY);
    if (srcFd == -1) {
        return -1;
    }
    int dstFd = openat(dst->dirFd, dst->name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        close(srcFd);
        return -1;
    }
    pthread_mutex_lock(progressMutex);
    *progress->flags = PROGRESS_COPY;
    strcpy(src->name, progress->srcName);
    strcpy(dst->name, progress->dstName);
    pthread_mutex_unlock(progressMutex);

    // 복사 수행
    int result = doCopyFile(srcFd, dstFd, src->fileSize, progress, progressMutex);

    close(srcFd);
    close(dstFd);

    if (result == -1) {
        unlinkat(dst->dirFd, dst->name, 0);
    }
    return result;
}

int moveFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress, pthread_mutex_t *progressMutex) {
    // 같은 디바이스인 경우, 진행률 표시 X
    // 다른 디바이스인 경우, 복사 함수를 통해 진행률 표시
    pthread_mutex_lock(progressMutex);
    *progress->flags = PROGRESS_MOVE;
    strcpy(src->name, progress->srcName);
    strcpy(dst->name, progress->dstName);
    pthread_mutex_unlock(progressMutex);

    // 같은 디바이스면 rename
    if (src->devNo == dst->devNo) {
        if (renameat(src->dirFd, src->name, dst->dirFd, dst->name) == 0) {
            return 0;
        }
    }

    // 다른 디바이스면 복사 후 원본 삭제
    int srcFd = openat(src->dirFd, src->name, O_RDONLY);
    if (srcFd == -1) {
        return -1;
    }
    int dstFd = openat(dst->dirFd, dst->name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        close(srcFd);
        return -1;
    }

    // 복사 수행
    int result = doCopyFile(srcFd, dstFd, src->fileSize, progress, progressMutex);

    close(srcFd);
    close(dstFd);

    if (result == -1) {
        return unlinkat(dst->dirFd, dst->name, 0);
    }
    return unlinkat(src->dirFd, src->name, 0);
}

int removeFile(SrcDstInfo *src, FileProgressInfo *progress, pthread_mutex_t *progressMutex) {
    // 삭제: atomic 작업 -> 시작만 표시
    pthread_mutex_lock(progressMutex);
    *progress->flags = PROGRESS_DELETE;
    strcpy(src->name, progress->srcName);
    pthread_mutex_unlock(progressMutex);
    return unlinkat(src->dirFd, src->name, 0);
}