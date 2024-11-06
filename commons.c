#include <stdint.h>
#include <time.h>

#include "commons.h"


uint64_t getElapsedTime(struct timespec baseTime) {
    // 현재 시간 가져옴
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);

    // 흐른 시간 계산 (밀리초 단위)
    uint64_t elapsedTime =
        (uint64_t)((currentTime.tv_sec - baseTime.tv_sec) * 1000 * 1000ULL)  // 초 단위 흐른 시간: 단위 변환 후 더함
        + (currentTime.tv_nsec - baseTime.tv_nsec) / 1000;  // 나노초 단위 흐른 시간: 단위 변환 후 더함

    return elapsedTime;
}
