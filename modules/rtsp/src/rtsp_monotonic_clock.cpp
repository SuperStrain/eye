#include "rtsp_monotonic_clock.h"

#include <time.h>

static uint64_t rtsp_monotonic_now_ns() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
        static_cast<uint64_t>(ts.tv_nsec);
}

uint64_t rtsp_monotonic_now_ms() {
    return rtsp_monotonic_now_ns() / 1000000ULL;
}

uint64_t rtsp_monotonic_now_us() {
    return rtsp_monotonic_now_ns() / 1000ULL;
}

int64_t rtsp_monotonic_elapsed_ms(uint64_t start_ms, uint64_t now_ms) {
    if (now_ms < start_ms) return 0;
    return static_cast<int64_t>(now_ms - start_ms);
}

uint64_t rtsp_monotonic_elapsed_us(uint64_t start_us, uint64_t now_us) {
    if (now_us < start_us) return 0;
    return now_us - start_us;
}
