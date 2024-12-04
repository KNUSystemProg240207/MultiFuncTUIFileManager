#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"
#include "file_functions.h"
#include "file_operator.h"


static unsigned int threadCnt = 0;  // 생성된 Thread 개수

int fileOperator(void *argsPtr);


int startFileOperator(pthread_t *newThread, FileOperatorArgs *args) {
    if (threadCnt >= MAX_FILE_OPERATORS)
        return -1;
    if (startThread(
            newThread, NULL, fileOperator, NULL,
            0, &args->commonArgs, args
        ) == -1) {
        return -1;
    }
    return ++threadCnt;
}

int fileOperator(void *argsPtr) {
    FileOperatorArgs *args = (FileOperatorArgs *)argsPtr;
    FileTask command;

    pthread_mutex_lock(args->pipeReadMutex);
    int ret = read(args->pipeEnd, &command, sizeof(FileTask));
    pthread_mutex_unlock(args->pipeReadMutex);
    switch (ret) {
        case 0:  // EOF: Write End가 Close됨 -> 종료
            pthread_mutex_lock(&args->commonArgs.statusMutex);
            args->commonArgs.statusFlags |= THREAD_FLAG_STOP;
            pthread_mutex_unlock(&args->commonArgs.statusMutex);
            return 0;
        case sizeof(FileTask):  // 성공적으로 읽어들임
            break;  // 계속 진행
        case -1:  // 읽기 실패
        default:  // Size 이상 있음
            return -1;  // abort
    }

    switch (command.type) {
        case COPY:
            copyFile(&command.src, &command.dst, args->progressInfo);
            if (command.src.dirFd != command.dst.dirFd)
                close(command.dst.dirFd);
            break;
        case MOVE:
            moveFile(&command.src, &command.dst, args->progressInfo);
            if (command.src.dirFd != command.dst.dirFd)
                close(command.dst.dirFd);
            break;
        case DELETE:
            removeFile(&command.src, args->progressInfo);
            // DELETE: dst.dirFd 유효하지 않음 -> close()하면 안 됨
            break;
        case MKDIR:
            makeDirectory(&command.src, args->progressInfo);
            // MKDIR: dst.dirFd 유효하지 않음 -> close()하면 안 됨
            break;
    }

    close(command.src.dirFd);  // 항상 쓰임 -> 항상 close()

    return 0;
}
