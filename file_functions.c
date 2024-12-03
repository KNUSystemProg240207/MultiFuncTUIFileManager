#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "file_functions.h"

#define COPY_CHUNK_SIZE (1024 * 1024)  // 1MB 단위로 복사

#define SET_FILE_OPERATION_FLAG(progress, fileName, operationType) \
    do { \
        pthread_mutex_lock(&(progress)->flagMutex); \
        (progress)->flags = operationType; \
        strcpy((progress)->name, (fileName)); \
        pthread_mutex_unlock(&(progress)->flagMutex); \
    } while (0)
#define CLEAR_FILE_OPERATION_FLAG(progress) \
    do { \
        pthread_mutex_lock(&(progress)->flagMutex); \
        (progress)->flags = 0; \
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
    off_t off_in = 0, off_out = 0;
    ssize_t copied;
#else
    // 불가능할 시: read() -> write()로 fallback
    ssize_t readBytes, writtenBytes, totalWrittenBytes;
    char buf[COPY_FILE_BUF_SIZE];
#endif

    while (totalCopied < fileSize) {
#if defined(_GNU_SOURCE) && (__LP64__ || (defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64))
        copied = copy_file_range(srcFd, &off_in, dstFd, &off_out, COPY_CHUNK_SIZE, 0);
        if (copied == -1)
            return -1;
        if (copied == 0)
            break;
        totalCopied += copied;
#else
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
#endif

        // 진행률 업데이트
        pthread_mutex_lock(&progress->flagMutex);
        progress->flags &= ~PROGRESS_PERCENT_MASK;
        progress->flags |= (int)((double)totalCopied / fileSize * 100) << PROGRESS_PERCENT_START;
        pthread_mutex_unlock(&progress->flagMutex);
    }

    return (totalCopied == fileSize) ? 0 : -1;
}

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
                    ((entry->d_name[0] == '.') && (entry->d_name[1] == '\0'))
                    || ((entry->d_name[0] == '.') && (entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))
                )  // '.', '..' 건너뜀
                    continue;

                // 파일 크기 확인 필요 -> 항상 stat() 호출 필요
                if (fstatat(srcDirFd, entry->d_name, &entryStat, AT_SYMLINK_NOFOLLOW) == -1) {
                    int tmp = errno;
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
    SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_COPY);
    int ret = doCopy(src, dst, progress);
    CLEAR_FILE_OPERATION_FLAG(progress);
    return ret;
}

int moveFile(SrcDstInfo *src, SrcDstInfo *dst, FileProgressInfo *progress) {
    SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_MOVE);

    // 같은 디바이스면 rename
    if (src->devNo == dst->devNo && renameat(src->dirFd, src->name, dst->dirFd, dst->name) == 0) {
        CLEAR_FILE_OPERATION_FLAG(progress);
        return 0;
    }

    // 다른 디바이스면, 혹은 윗 단계 실패 시: 복사 시도, 성공 시 원본 삭제
    int ret = doCopy(src, dst, progress);
    if (ret == 0)
        ret = removeFile(src, progress);  // 폴더 삭제 -> 아래 함수 사용
    CLEAR_FILE_OPERATION_FLAG(progress);
    return ret;
}

int removeFile(SrcDstInfo *src, FileProgressInfo *progress) {
    int ret;
    if (!S_ISDIR(src->mode)) {
        // 폴더 아닌 것 삭제: blocking -> 시작만 표시
        SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_DELETE);
        ret = unlinkat(src->dirFd, src->name, 0);
        CLEAR_FILE_OPERATION_FLAG(progress);
        return ret;
    }
    // 폴더 삭제: 하위 항목 모두 삭제 후, 오류 없었으면 rmdir() 호출
    SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_DELETE);
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
            ((entry->d_name[0] == '.') && (entry->d_name[1] == '\0'))
            || ((entry->d_name[0] == '.') && (entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))
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
    SET_FILE_OPERATION_FLAG(progress, src->name, PROGRESS_DELETE);
    closedir(currentDir);
    if (failed)
        return -1;
    ret = unlinkat(src->dirFd, src->name, AT_REMOVEDIR);
    CLEAR_FILE_OPERATION_FLAG(progress);
    return ret;
}