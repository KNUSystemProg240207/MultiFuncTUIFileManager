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

    pthread_mutex_lock(&args->pipeReadMutex);
    int ret = read(args->pipeEnd, &command, sizeof(FileTask));
    pthread_mutex_unlock(&args->pipeReadMutex);
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
            // TODO: Implement here
            copyFile(&command.src, &command.dst, &args->progress, &args->progressMutex);
        case MOVE:
            // TODO: Implement here
            moveFile(&command.src, &command.dst, &args->progress, &args->progressMutex);
        case DELETE:
            // TODO: Implement here
            removeFile(&command.src, &args->progress, &args->progressMutex);
    }

    close(command.src.dirFd);
    close(command.dst.dirFd);

    return 0;
}
