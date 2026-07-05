#include "rv_stream_fetcher.h"
#include "logger.h"
#include "stream_frame.h"

#include <rk_mpi_mb.h>
#include <rk_mpi_venc.h>
#include <cstring>
#include <pthread.h>
#include <unistd.h>

static NaluType map_nalu_type(CodecType codec, const VENC_PACK_S& pack) {
    if (codec == CodecType::H264) {
        if (pack.DataType.enH264EType == H264E_NALU_IDRSLICE ||
            pack.DataType.enH264EType == H264E_NALU_ISLICE) {
            return NaluType::IDR_SLICE;
        }
        if (pack.DataType.enH264EType == H264E_NALU_PSLICE) {
            return NaluType::P_SLICE;
        }
    } else if (codec == CodecType::H265) {
        if (pack.DataType.enH265EType == H265E_NALU_IDRSLICE ||
            pack.DataType.enH265EType == H265E_NALU_ISLICE) {
            return NaluType::IDR_SLICE;
        }
        if (pack.DataType.enH265EType == H265E_NALU_PSLICE) {
            return NaluType::P_SLICE;
        }
    } else if (codec == CodecType::MJPEG) {
        return NaluType::IDR_SLICE;
    }
    return NaluType::OTHER;
}

RvStreamFetcher::RvStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                                 StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec), distributor_(distributor),
      running_(false) {}

RvStreamFetcher::~RvStreamFetcher() { stop(); }

int RvStreamFetcher::start() {
    if (running_) return 0;
    running_ = true;
    thread_ = std::thread(&RvStreamFetcher::run, this);
    return 0;
}

int RvStreamFetcher::stop() {
    if (!running_) return 0;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    return 0;
}

int RvStreamFetcher::fetchFrame(VencChannel chn, FrameData& frame) {
    int chn_val = static_cast<int>(chn);
    VENC_STREAM_S stream;
    VENC_PACK_S pack;
    std::memset(&stream, 0, sizeof(stream));
    std::memset(&pack, 0, sizeof(pack));
    stream.pstPack = &pack;
    stream.u32PackCount = 1;

    int ret = RK_MPI_VENC_GetStream(chn_val, &stream, 1000);
    if (ret != RK_SUCCESS) {
        return -1;
    }

    void* data = RK_MPI_MB_Handle2VirAddr(pack.pMbBlk);
    if (data == nullptr || pack.u32Len == 0) {
        RK_MPI_VENC_ReleaseStream(chn_val, &stream);
        return -1;
    }

    frame.pack_count = 1;
    frame.seq = 0;
    frame.packs[0].data = static_cast<const uint8_t*>(data);
    frame.packs[0].len = pack.u32Len;
    frame.packs[0].nalu_type = map_nalu_type(codec_type_, pack);

    auto stream_frame = std::make_shared<StreamFrame>(channel_, stream_type_, codec_type_, frame);
    ret = RK_MPI_VENC_ReleaseStream(chn_val, &stream);
    if (ret != RK_SUCCESS) {
        LOGGER_ERROR(STREAM, "RV1126B ch%d RK_MPI_VENC_ReleaseStream failed: %#x", chn_val, ret);
    }
    distributor_.push(stream_frame);
    return 0;
}

int RvStreamFetcher::releaseFrame(VencChannel) {
    return 0;
}

void RvStreamFetcher::run() {
    int chn_val = static_cast<int>(channel_);
    char name[16];
    snprintf(name, sizeof(name), "RvFetch_%d", chn_val);
    pthread_setname_np(pthread_self(), name);

    LOGGER_INFO(STREAM, "RV1126B fetcher ch%d running", chn_val);
    while (running_) {
        FrameData frame;
        if (fetchFrame(channel_, frame) != 0) {
            usleep(10 * 1000);
        }
    }
    LOGGER_INFO(STREAM, "RV1126B fetcher ch%d stopped", chn_val);
}
