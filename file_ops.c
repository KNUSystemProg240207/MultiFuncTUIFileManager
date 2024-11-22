#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>


#include "file_ops.h"

#define COPY_CHUNK_SIZE (1024 * 1024)  // 1MB 단위로 복사

/**
 * 실제 파일 복사를 수행하는 내부 함수
 * - copy_file_range 시스템 콜을 사용하여 커널 공간에서 직접 복사
 * - 청크 단위(1MB)로 분할하여 복사하며 진행률 업데이트
 * - 복사 작업의 재사용성을 높이기 위해 별도 함수로 분리
 *
 * @param srcFd 원본 파일 디스크립터
 * @param dstFd 대상 파일 디스크립터
 * @param fileSize 전체 파일 크기
 * @param progress 진행률 포인터
 * @param progressMutex 진행률 보호 뮤텍스
 * @param progressFlag 작업 종류 플래그 (PROGRESS_COPY/PROGRESS_MOVE)
 * @return 성공: 0, 실패: -1
 */
static int doCopyFile(int srcFd, int dstFd, size_t fileSize, 
                     uint16_t *progress, pthread_mutex_t progressMutex,
                     uint16_t progressFlag) {
    loff_t off_in = 0, off_out = 0;
    ssize_t bytes_copied;
    size_t remaining = fileSize;
    size_t total_copied = 0;

    while (remaining > 0) {
        size_t chunk = (remaining > COPY_CHUNK_SIZE) ? COPY_CHUNK_SIZE : remaining;
        bytes_copied = copy_file_range(srcFd, &off_in,
                                     dstFd, &off_out,
                                     chunk, 0);
        if (bytes_copied <= 0) {
            if (errno == EINVAL || errno == EXDEV) {
                return -1;
            }
            return -1;
        }

        total_copied += bytes_copied;
        remaining -= bytes_copied;

        // 진행률 업데이트
        pthread_mutex_lock(&progressMutex);
        *progress = progressFlag | ((total_copied * 100) / fileSize);
        pthread_mutex_unlock(&progressMutex);
    }

    return (remaining == 0) ? 0 : -1;
}

int copyFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t progressMutex) {
    int srcFd, dstFd;

    srcFd = openat(src->dirFd, src->name, O_RDONLY);
    if (srcFd == -1) return -1;

    dstFd = openat(dst->dirFd, dst->name, 
        O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        close(srcFd);
        return -1;
    }

    // 복사 수행
    int result = doCopyFile(srcFd, dstFd, src->fileSize, 
                           progress, progressMutex, PROGRESS_COPY);

    close(srcFd);
    close(dstFd);

    if (result == -1) {
        unlinkat(dst->dirFd, dst->name, 0);
    }

    return result;
}

// ===== 변경사항 3: 작업 종류를 인자로 받아 진행률 표시 =====
// copyFile과 moveFile에서 모두 사용할 수 있도록
// progressFlag를 인자로 받아 작업 종류에 따라 다른 진행률 표시
// 예: copyFile에서는 PROGRESS_COPY를,
//     moveFile에서는 PROGRESS_MOVE를 전달

// ===== 변경사항 4: move/delete의 진행률 표시 단순화 =====
int moveFile(SrcDstInfo *src, SrcDstInfo *dst, uint16_t *progress, pthread_mutex_t progressMutex) {
    // 같은 디바이스인 경우 renameat 사용시
    // 진행률은 시작(0%)과 완료(100%)만 표시
    // 다른 디바이스인 경우 복사 함수를 통해 진행률 표시
    pthread_mutex_lock(&progressMutex);
    *progress = PROGRESS_MOVE; // 시작 표시
    pthread_mutex_unlock(&progressMutex);
    // ... 구현 내용 ...

    // 같은 디바이스면 rename
    if (src->devNo == dst->devNo) {
        if (renameat(src->dirFd, src->name, 
            dst->dirFd, dst->name) == 0) {
            pthread_mutex_lock(&progressMutex);
            *progress = PROGRESS_MOVE | 100;
            pthread_mutex_unlock(&progressMutex);
            return 0;
        }
    }

    // 다른 디바이스면 복사 후 원본 삭제
    int srcFd = openat(src->dirFd, src->name, O_RDONLY);
    if (srcFd == -1) return -1;

    int dstFd = openat(dst->dirFd, dst->name, 
        O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dstFd == -1) {
        close(srcFd);
        return -1;
    }

    // 복사 수행
    int result = doCopyFile(srcFd, dstFd, src->fileSize, 
                           progress, progressMutex, PROGRESS_MOVE);

    close(srcFd);
    close(dstFd);

    if (result == -1) {
        unlinkat(dst->dirFd, dst->name, 0);
        return -1;
    }

    // 복사 성공 후 원본 삭제
    // ===== 변경사항 5: 원본 삭제 실패시 복사본 유지 =====
    // moveFile 함수에서 복사 후 원본 삭제 실패시
    // 이전에는 복사본도 삭제했으나,
    // 수정 후에는 복사본을 유지하도록 변경
    // 사용자가 원본과 복사본을 모두 가질 수 있음
    if (unlinkat(src->dirFd, src->name, 0) == -1) {
        return -1;  // 원본 삭제 실패해도 복사본은 유지
    }

    return 0;
}

int removeFile(SrcDstInfo *src, uint16_t *progress, pthread_mutex_t progressMutex) {
    // 삭제는 atomic 작업이므로 시작과 완료만 표시
    pthread_mutex_lock(&progressMutex);
    *progress = PROGRESS_DELETE; // 시작 표시
    pthread_mutex_unlock(&progressMutex);
    // ... 구현 내용 ...

    int result = unlinkat(src->dirFd, src->name, 0);

    if (result == 0) {
        pthread_mutex_lock(&progressMutex);
        *progress = PROGRESS_DELETE | 100;
        pthread_mutex_unlock(&progressMutex);
    }

    return result;
}