#include "rtsp_parameter_set_cache.h"

bool ParameterSetCache::has_sps_pps() const {
    return !sps.empty() && !pps.empty();
}

bool ParameterSetCache::has_vps_sps_pps() const {
    return !vps.empty() && !sps.empty() && !pps.empty();
}
