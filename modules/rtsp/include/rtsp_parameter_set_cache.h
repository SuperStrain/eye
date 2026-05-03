#ifndef RTSP_PARAMETER_SET_CACHE_H
#define RTSP_PARAMETER_SET_CACHE_H

#include <cstdint>
#include <mutex>
#include <vector>

struct ParameterSetCache {
    std::vector<uint8_t> vps;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    std::mutex mutex;

    bool has_sps_pps() const;
    bool has_vps_sps_pps() const;
};

#endif
