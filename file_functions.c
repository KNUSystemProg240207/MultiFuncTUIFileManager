#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "file_functions.h"
#include "file_operator.h"

#define COPY_CHUNK_SIZE (1024 * 1024)  // 1MB 단위로 복사

#define FILEOP_SET_OPERATION(progress, fileName, operationFlag) \
    do { \
        pthread_mutex_lock(&(progress)->flagMutex); \
        (progress)->flags |= (operationFlag); \
        (progress)->flags &= ~PROGRESS_PERCENT_MASK; \
        strcpy((progress)->name, (fileName)); \
        pthread_mutex_unlock(&(progress)->flagMutex); \
    } while (0)
#define FILEOP_SET_RESULT(progress, operationFlag, isFailed) \
    do { \
        pthread_mutex_lock(&(progress)->flagMutex); \
        (progress)->flags = ((isFailed) ? ((operationFlag) | PROGRESS_PREV_FAIL) : (operationFlag)); \
        pthread_mutex_unlock(&(progress)->flagMutex); \
    } while (0)


extern int directoryOpenArgs;


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
    size_t totalCopied = 0;
#if defined(_GNU_SOURCE) && (__LP64__ || (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64))
    // copy_file_range 사용 가능하면 사용 (kernel에서 바로 복사 -> 성능상 유리)
    off_t offIn = 0, offOut = 0;
    ssize_t copied = 0;
#endif
    // 불가능할 or 실패 시: read() -> write()로 fallback
    ssize_t readBytes, writtenBytes, totalWrittenBytes;
    char buf[COPY_FILE_BUF_SIZE];

#if defined(_GNU_SOURCE) && (__LP64__ || (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64))
    while (totalCopied < fileSize) {
        copied = copy_file_range(srcFd, &offIn, dstFd, &offOut, COPY_CHUNK_SIZE, 0);
        if (
            copied == 0  // EOF 만남
            || copied == -1  // copy_file_range 실패
        )
            break;
        totalCopied += copied;

        // 진행률 업데이트
        pthread_mutex_lock(&progress->flagMutex);
        progress->flags &= ~PROGRESS_PERCENT_MASK;
        progress->flags |= (int)((double)totalCopied / fileSize * 100) << PROGRESS_PERCENT_START;
        pthread_mutex_unlock(&progress->flagMutex);
    }
    if (copied != -1) {
        return (totalCopied == fileSize) ? 0 : -1;
    }
#endif

    // copy_file_range 실패 시 -> read-and-write로 fallback

    // 복사 성공한 위치로 seek()
    lseek(srcFd, offIn, SEEK_SET);
    lseek(dstFd, offOut, SEEK_SET);

    // 복사 수행
    while (totalCopied < fileSize) {
        readBytes = read(srcFd, buf, COPY_FILE_BUF_SIZE);
        if (readBytes == -1)
            return -1;
        if (readBytes == 0)
            break;
        totalWrittenBytes = 0;
        while (totalWrittenBytes < readBytes) {
            writtenBytes = write(dstFd, &buf[totalWrittenBytes], readBytes - totalWrittenBytes);
            if (writtenBytes == -1)
                return -1;
            totalWrittenBytes += writtenBytes;
        }
        totalCopied += readBytes;

        // 진행률 업데이트
        pthread_mutex_lock(&progress->flagMutex);
        progress->flags &= ~PROGRESS_PERCENT_MASK;
        progress->flags |= (int)((double)totalCopied / fileSize * 100) << PROGRESS_PERCENT_START;
        pthread_mutex_unlock(&progress->flagMutex);
    }

    return (totalCopied == fileSize) ? 0 : -1;
}

/**
 * (공통 기능 함수) 파일/폴더 복사 수행 (폴더: 재귀호출로 복사)
 *
 * @param src 원본 파일
 * @param dst 대상 폴더
 * @param progress 진행 상태 구조체
 * @return 성공: 0, 실패: -1
 */
static inline int doCopy(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress) {
    int ret;
    switch (src->mode & S_IFMT) {
        case S_IFREG:
            // 일반 파일 -> 복사 수행
            int srcFd = openat(src->dirFd, src->name, O_RDONLY);
            if (srcFd == -1) {
                return -1;
            }
            int dstFd = openat(dst->dirFd, dst->name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dstFd == -1) {
                close(srcFd);
                return -1;
            }
            ret = doCopyFile(srcFd, dstFd, src->fileSize, progress);
            close(srcFd);
            close(dstFd);
            if (ret == -1)
                unlinkat(dst->dirFd, dst->name, 0);
            return ret;
        case S_IFDIR:
            // 폴더 -> 재귀적으로 복사
            if (mkdirat(dst->dirFd, dst->name, src->mode) == -1)  // 폴더 생성 실패 -> 내용물 복사 불가
                return -1;

            // 필요한 Descriptor open()
            int srcDirFd = openat(src->dirFd, src->name, directoryOpenArgs);
            if (srcDirFd == -1)
                return -1;
            int dstDirFd = openat(dst->dirFd, dst->name, directoryOpenArgs);
            if (dstDirFd == -1) {
                close(srcDirFd);
                return -1;
            }
            DIR *srcDir = fdopendir(srcDirFd);
            if (srcDir == NULL) {
                close(srcDirFd);
                close(dstDirFd);
                return -1;
            }

            // 하위 항목 재귀적으로 복사
            bool failed = false;
            SrcDstInfo childSrc, childDst;
            struct stat entryStat;
            for (struct dirent *entry = readdir(srcDir); entry != NULL; entry = readdir(srcDir)) {
                if (
                    ((entry->d_name[0] == '.') && (entry->d_name[1] == '\0')) || ((entry->d_name[0] == '.') && (entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))
                )  // '.', '..' 건너뜀
                    continue;

                // 파일 크기 확인 필요 -> 항상 stat() 호출 필요
                if (fstatat(srcDirFd, entry->d_name, &entryStat, AT_SYMLINK_NOFOLLOW) == -1) {
                    failed = true;
                    continue;
                }
                childSrc.devNo = -1;  // 이 함수에서는 미사용
                childSrc.mode = entryStat.st_mode;
                childSrc.dirFd = srcDirFd;
                strcpy(childSrc.name, entry->d_name);
                childSrc.fileSize = entryStat.st_size;

                childDst = childSrc;  // 나머지 정보는 같음
                childDst.dirFd = dstDirFd;  // 목적지 폴더 설정
                if (doCopy(&childSrc, &childDst, progress) == -1) {  // 하위 Item 대해 Recursive하게 복사 시도
                    failed = true;
                }
            }
            closedir(srcDir);
            close(dstDirFd);
            if (failed)
                return -1;
            return 0;
        default:
            // 다른 종류: 지원 X
            return -1;
    }
}

int copyFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress) {
    FILEOP_SET_OPERATION(progress, src->name, PROGRESS_OP_CP);
    int ret = doCopy(src, dst, progress);
    FILEOP_SET_RESULT(progress, PROGRESS_PREV_CP, ret == -1);
    return ret;
}

int moveFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress) {
    FILEOP_SET_OPERATION(progress, src->name, PROGRESS_OP_MV);

    // 같은 디바이스면 rename
    if (src->devNo == dst->devNo && renameat(src->dirFd, src->name, dst->dirFd, dst->name) == 0) {
        FILEOP_SET_RESULT(progress, PROGRESS_PREV_MV, false);
        return 0;
    }

    // 다른 디바이스면, 혹은 윗 단계 실패 시: 복사 시도
    int ret = doCopy(src, dst, progress);
    if (ret == 0)
        ret = removeFile(src, progress);  // 성공 시: 원본 삭제 (아래 함수 사용)
    FILEOP_SET_RESULT(progress, PROGRESS_PREV_MV, ret == -1);
    return ret;
}

int removeFile(SrcDstInfo *src, FileProgressInfo *progress) {
    int ret;
    FILEOP_SET_OPERATION(progress, src->name, PROGRESS_OP_RM);

    if (!S_ISDIR(src->mode)) {
        ret = unlinkat(src->dirFd, src->name, 0);
        FILEOP_SET_RESULT(progress, PROGRESS_PREV_RM, ret == -1);
        return ret;
    }

    // 폴더 삭제: 하위 항목 모두 삭제 후, 오류 없었으면 rmdir() 호출
    int curDirFd = openat(src->dirFd, src->name, directoryOpenArgs);
    if (curDirFd == -1)
        return -1;
    DIR *currentDir = fdopendir(curDirFd);
    if (currentDir == NULL) {
        close(curDirFd);
        return -1;
    }

    bool failed = false;
    SrcDstInfo childInfo;
    struct stat entryStat;
    for (struct dirent *entry = readdir(currentDir); entry != NULL; entry = readdir(currentDir)) {
        if (
            ((entry->d_name[0] == '.') && (entry->d_name[1] == '\0')) || ((entry->d_name[0] == '.') && (entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))
        )  // '.', '..' 건너뜀
            continue;
        childInfo.devNo = -1;  // 이 함수에서는 안 쓰임
        childInfo.dirFd = curDirFd;
        strcpy(childInfo.name, entry->d_name);
        childInfo.fileSize = -1;  // 이 함수에서는 안 쓰임

        // 이 함수에서는 파일 종류만 사용
        switch (entry->d_type) {
#if defined(DT_BLK) && defined(S_IFBLK)
            case DT_BLK:
                childInfo.mode = S_IFBLK;
                break;
#endif
#if defined(DT_CHR) && defined(S_IFCHR)
            case DT_CHR:
                childInfo.mode = S_IFCHR;
                break;
#endif
#if defined(DT_DIR) && defined(S_IFDIR)
            case DT_DIR:
                childInfo.mode = S_IFDIR;
                break;
#endif
#if defined(DT_FIFO) && defined(S_IFIFO)
            case DT_FIFO:
                childInfo.mode = S_IFIFO;
                break;
#endif
#if defined(DT_LNK) && defined(S_IFLNK)
            case DT_LNK:
                childInfo.mode = S_IFLNK;
                break;
#endif
#if defined(DT_REG) && defined(S_IFREG)
            case DT_REG:
                childInfo.mode = S_IFREG;
                break;
#endif
#if defined(DT_SOCK) && defined(S_IFSOCK)
            case DT_SOCK:
                childInfo.mode = S_IFSOCK;
                break;
#endif
            default:  // 아마도 DT_UNKNOWN -> stat()으로 파일 종류 확인
                if (fstatat(curDirFd, entry->d_name, &entryStat, AT_SYMLINK_NOFOLLOW) == -1) {
                    failed = true;
                    continue;
                }
                childInfo.mode = entryStat.st_mode;
        }

        if (removeFile(&childInfo, progress) == -1) {  // 하위 Item 대해 Recursive하게 삭제 시도
            failed = true;
        }
    }
    FILEOP_SET_OPERATION(progress, src->name, PROGRESS_OP_RM);
    closedir(currentDir);
    if (failed) {
        FILEOP_SET_RESULT(progress, PROGRESS_PREV_RM, true);
        return -1;
    }
    ret = unlinkat(src->dirFd, src->name, AT_REMOVEDIR);
    FILEOP_SET_RESULT(progress, PROGRESS_PREV_RM, ret == -1);
    return ret;
}

int makeDirectory(SrcDstInfo *src, FileProgressInfo *progress) {
    FILEOP_SET_OPERATION(progress, src->name, PROGRESS_OP_MKDIR);
    int ret = mkdirat(src->dirFd, src->name, 0755);
    FILEOP_SET_RESULT(progress, PROGRESS_PREV_MKDIR, ret == -1);
    return ret;
}
