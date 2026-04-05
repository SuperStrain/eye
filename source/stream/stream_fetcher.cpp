#include "stream_fetcher.h"
#include "logger.h"
#include <ss_mpi_venc.h>
#include <pthread.h>
#include <sys/select.h>
#include <cstring>

StreamFetcher::StreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                             StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec),
      distributor_(distributor) {}

StreamFetcher::~StreamFetcher() {
    stop();
}

void StreamFetcher::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&StreamFetcher::run, this);
    LOGGER_NOTICE(STREAM, "Fetcher ch%d started", static_cast<int>(channel_));
}

void StreamFetcher::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    LOGGER_NOTICE(STREAM, "Fetcher ch%d stopped", static_cast<int>(channel_));
}

void StreamFetcher::run() {
    int chn_val = static_cast<int>(channel_);
    int fd = ss_mpi_venc_get_fd(chn_val);
    if (fd < 0) {
        LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_get_fd failed, fd=%d", chn_val, fd);
        return;
    }
    LOGGER_INFO(STREAM, "Fetcher ch%d running, fd=%d", chn_val, fd);

    char name[16];
    snprintf(name, sizeof(name), "Fetcher_ch%d", chn_val);
    pthread_setname_np(pthread_self(), name);

    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        timeval tv = {1, 0};

        int ret = select(fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

        ot_venc_chn_status stat;
        if (ss_mpi_venc_query_status(chn_val, &stat) != TD_SUCCESS) {
            LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_query_status failed", chn_val);
            continue;
        }

        if (stat.cur_packs == 0) {
            continue;
        }

        ot_venc_stream stream;
        stream.pack = (ot_venc_pack*)malloc(sizeof(ot_venc_pack) * stat.cur_packs);
        if (stream.pack == nullptr) {
            LOGGER_ERROR(STREAM, "Fetcher ch%d: malloc pack failed, count=%u", chn_val, stat.cur_packs);
            continue;
        }
        stream.pack_cnt = stat.cur_packs;

        td_s32 ok = ss_mpi_venc_get_stream(chn_val, &stream, 1000);
        if (ok != 0) {
            LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_get_stream failed, ret=%d", chn_val, ok);
            free(stream.pack);
            continue;
        }

        auto frame = std::make_shared<StreamFrame>(channel_, stream_type_, codec_type_, stream);
        td_s32 release_ret = ss_mpi_venc_release_stream(chn_val, &stream);
        if (release_ret != TD_SUCCESS) {
            LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_release_stream failed, ret=%#x", chn_val, release_ret);
        }
        free(stream.pack);
        stream.pack = nullptr;
        distributor_.push(frame);
    }
}
