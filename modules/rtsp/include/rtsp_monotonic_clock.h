#ifndef RTSP_MONOTONIC_CLOCK_H
#define RTSP_MONOTONIC_CLOCK_H

#include <cstdint>

uint64_t rtsp_monotonic_now_ms();
uint64_t rtsp_monotonic_now_us();
int64_t rtsp_monotonic_elapsed_ms(uint64_t start_ms, uint64_t now_ms);
uint64_t rtsp_monotonic_elapsed_us(uint64_t start_us, uint64_t now_us);

#endif
