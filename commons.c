#include <stdint.h>
#include <time.h>

#include "commons.h"


uint64_t getElapsedTime(struct timespec baseTime) {
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC, &currentTime);

    uint64_t elapsedUSec = (currentTime.tv_nsec - baseTime.tv_nsec) / 1000;
    elapsedUSec += (currentTime.tv_sec - baseTime.tv_sec) * 1000 * 1000;

    return elapsedUSec;
}
