#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>


#include "file_ops.h"

#define COPY_CHUNK_SIZE (1024 * 1024)  // 1MB 단위로 복사

int copyFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t progressMutex) {
    int srcFd, dstFd;
    int result = -1;

    // 소스 파일 열기
    srcFd = openat(src->dirFd, src->name, O_RDONLY);
    if (srcFd == -1) return -1;

    // 대상 파일 생성
    dstFd = openat(dst->dirFd, dst->name, 
        O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        close(srcFd);
        return -1;
    }

    // 복사 진행 상태 초기화
    pthread_mutex_lock(&progressMutex);
    *progress = (1 << 8);  // 복사 시작 표시 (8번 비트 설정)
    pthread_mutex_unlock(&progressMutex);

    // copy_file_range를 사용하여 파일 복사
    loff_t off_in = 0, off_out = 0;
    ssize_t bytes_copied;
    size_t remaining = src->fileSize;
    size_t total_copied = 0;

    while (remaining > 0) {
        size_t chunk = (remaining > COPY_CHUNK_SIZE) ? COPY_CHUNK_SIZE : remaining;
        bytes_copied = copy_file_range(srcFd, &off_in,
                                     dstFd, &off_out,
                                     chunk, 0);
        if (bytes_copied <= 0) {
            if (errno == EINVAL || errno == EXDEV) {
                // copy_file_range가 지원되지 않는 경우
                result = -1;
                break;
            }
            result = -1;
            break;
        }

        total_copied += bytes_copied;
        remaining -= bytes_copied;

        // 진행률 업데이트 (0-100%)
        pthread_mutex_lock(&progressMutex);
        *progress = (1 << 8) | ((total_copied * 100) / src->fileSize);
        pthread_mutex_unlock(&progressMutex);
    }

    // 파일 디스크립터 정리
    close(srcFd);
    close(dstFd);

    if (result == -1) {
        unlinkat(dst->dirFd, dst->name, 0);  // 실패시 대상 파일 삭제
    }

    return (remaining == 0) ? 0 : -1;
}

int moveFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t progressMutex) {
    // 이동 시작 표시
    pthread_mutex_lock(&progressMutex);
    *progress = (1 << 9);  // 이동 시작 표시 (9번 비트 설정)
    pthread_mutex_unlock(&progressMutex);

    // 같은 디바이스인 경우: renameat 시도
    if (src->devNo == dst->devNo) {
        if (renameat(src->dirFd, src->name, 
            dst->dirFd, dst->name) == 0) {
            // 이동 완료 표시
            pthread_mutex_lock(&progressMutex);
            *progress = (1 << 9) | 100;
            pthread_mutex_unlock(&progressMutex);
            return 0;
        }
        // rename 실패하면 복사+삭제로 진행
    }

    // 다른 디바이스이거나 rename 실패한 경우: 복사 후 원본 삭제
    if (copyFile(src, dst, progress, progressMutex) == -1) {
        return -1;
    }

    // 원본 파일 삭제
    if (unlinkat(src->dirFd, src->name, 0) == -1) {
        // 원본 삭제 실패 시 복사본도 삭제
        unlinkat(dst->dirFd, dst->name, 0);
        return -1;
    }

    // 이동 완료 표시
    pthread_mutex_lock(&progressMutex);
    *progress = (1 << 9) | 100;
    pthread_mutex_unlock(&progressMutex);

    return 0;
}

int removeFile(SrcDstInfo *src, uint16_t *progress, pthread_mutex_t progressMutex) {
    // 삭제 시작 표시
    pthread_mutex_lock(&progressMutex);
    *progress = (1 << 10);  // 삭제 시작 표시 (10번 비트 설정)
    pthread_mutex_unlock(&progressMutex);

    int result = unlinkat(src->dirFd, src->name, 0);

    // 삭제 완료 표시
    pthread_mutex_lock(&progressMutex);
    *progress = (1 << 10) | (result == 0 ? 100 : 0);
    pthread_mutex_unlock(&progressMutex);

    return result;
}