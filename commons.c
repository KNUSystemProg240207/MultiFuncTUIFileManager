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

char *formatSize(size_t size) {
    static char formatted_size[16];
    const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    int unit_index = 0;

    while (size >= (1 << 10) && unit_index < 4) {
        size >>= 10;  // 성능을 위해 비트 시프트로 연산, 2^10을 나누는 나눔
        unit_index++;
    }
    // 사이즈 형식으로 포매팅해서 스트링 리턴
    snprintf(formatted_size, sizeof(formatted_size), "%lu%s", size, units[unit_index]);
    return formatted_size;
}
