#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

#include "config.h"
#include "file_operator.h"


static unsigned int threadCnt = 0;  // 생성된 Thread 개수

int fileOperator(void *argsPtr);


int startFileOperator(pthread_t *newThread, FileOperatorArgs *args) {
    if (threadCnt >= MAX_FILE_OPERATORS)
        return -1;
    if (startThread(
            newThread, NULL, fileOperator, NULL,
            DIR_INTERVAL_USEC, &args->commonArgs, args
        ) == -1) {
        return -1;
    }
    return ++threadCnt;
}

int fileOperator(void *argsPtr) {
    FileOperatorArgs *args = (FileOperatorArgs *)argsPtr;
    FileTask command;

    read(args->pipeEnd, &command, sizeof(FileTask));

    switch (command.type) {
        case COPY:
            // TODO: Implement here
            break;
        case MOVE:
            // TODO: Implement here
            break;
        case DELETE:
            // TODO: Implement here
            break;
    }

    return 0;
}
