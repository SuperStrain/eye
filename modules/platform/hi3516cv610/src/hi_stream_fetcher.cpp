#include "hi_stream_fetcher.h"
#include "logger.h"
#include <ss_mpi_venc.h>
#include <pthread.h>
#include <sys/select.h>
#include <cstring>

StreamFetcher::StreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                             StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec),
      distributor_(distributor), fd_(-1) {}

StreamFetcher::~StreamFetcher() {
    stop();
}

int StreamFetcher::start() {
    if (running_) return 0;
    running_ = true;
    thread_ = std::thread(&StreamFetcher::run, this);
    LOGGER_NOTICE(STREAM, "Fetcher ch%d started", static_cast<int>(channel_));
    return 0;
}

int StreamFetcher::stop() {
    if (!running_) return 0;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    LOGGER_NOTICE(STREAM, "Fetcher ch%d stopped", static_cast<int>(channel_));
    return 0;
}

int StreamFetcher::fetchFrame(VencChannel chn, FrameData& frame) {
    int chn_val = static_cast<int>(chn);

    ot_venc_chn_status stat;
    if (ss_mpi_venc_query_status(chn_val, &stat) != TD_SUCCESS) {
        return -1;
    }
    if (stat.cur_packs == 0) {
        return -1;
    }
    if (stat.cur_packs > FrameData::MAX_PACKS) {
        LOGGER_ERROR(STREAM, "Fetcher ch%d: cur_packs(%u) exceeds MAX_PACKS(%u), truncated",
                     chn_val, stat.cur_packs, FrameData::MAX_PACKS);
        stat.cur_packs = FrameData::MAX_PACKS;
    }

    current_stream_.pack = (ot_venc_pack*)malloc(sizeof(ot_venc_pack) * stat.cur_packs);
    if (current_stream_.pack == nullptr) {
        return -1;
    }
    current_stream_.pack_cnt = stat.cur_packs;

    td_s32 ok = ss_mpi_venc_get_stream(chn_val, &current_stream_, 1000);
    if (ok != 0) {
        free(current_stream_.pack);
        current_stream_.pack = nullptr;
        return -1;
    }

    frame.pack_count = current_stream_.pack_cnt;
    frame.seq = 0;

    for (uint32_t i = 0; i < current_stream_.pack_cnt; ++i) {
        const auto& src_pack = current_stream_.pack[i];
        td_u32 offset = src_pack.offset;
        td_u32 data_len = src_pack.len - offset;

        NaluType nalu_type = NaluType::OTHER;
        if (codec_type_ == CodecType::H264) {
            if (src_pack.data_type.h264_type == OT_VENC_H264_NALU_IDR_SLICE) {
                nalu_type = NaluType::IDR_SLICE;
            } else if (src_pack.data_type.h264_type == OT_VENC_H264_NALU_P_SLICE) {
                nalu_type = NaluType::P_SLICE;
            } else if (src_pack.data_type.h264_type == OT_VENC_H264_NALU_B_SLICE) {
                nalu_type = NaluType::B_SLICE;
            }
        } else if (codec_type_ == CodecType::H265) {
            if (src_pack.data_type.h265_type == OT_VENC_H265_NALU_IDR_SLICE) {
                nalu_type = NaluType::IDR_SLICE;
            } else if (src_pack.data_type.h265_type == OT_VENC_H265_NALU_P_SLICE) {
                nalu_type = NaluType::P_SLICE;
            } else if (src_pack.data_type.h265_type == OT_VENC_H265_NALU_B_SLICE) {
                nalu_type = NaluType::B_SLICE;
            }
        }

        frame.packs[i].data = (const uint8_t*)src_pack.addr + offset;
        frame.packs[i].len = data_len;
        frame.packs[i].nalu_type = nalu_type;
    }

    return 0;
}

int StreamFetcher::releaseFrame(VencChannel chn) {
    int chn_val = static_cast<int>(chn);
    if (current_stream_.pack != nullptr) {
        td_s32 release_ret = ss_mpi_venc_release_stream(chn_val, &current_stream_);
        if (release_ret != TD_SUCCESS) {
            LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_release_stream failed, ret=%#x", chn_val, release_ret);
        }
        free(current_stream_.pack);
        current_stream_.pack = nullptr;
    }
    return 0;
}

void StreamFetcher::run() {
    int chn_val = static_cast<int>(channel_);
    fd_ = ss_mpi_venc_get_fd(chn_val);
    if (fd_ < 0) {
        LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_get_fd failed, fd=%d", chn_val, fd_);
        return;
    }
    LOGGER_INFO(STREAM, "Fetcher ch%d running, fd=%d", chn_val, fd_);

    char name[16];
    snprintf(name, sizeof(name), "Fetcher_ch%d", chn_val);
    pthread_setname_np(pthread_self(), name);

    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        timeval tv = {1, 0};

        int ret = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

        FrameData frame;
        if (fetchFrame(channel_, frame) == 0) {
            auto stream_frame = std::make_shared<StreamFrame>(channel_, stream_type_, codec_type_, frame);
            releaseFrame(channel_);
            distributor_.push(stream_frame);
        }
    }
}
