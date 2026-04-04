#include "stream_fetcher.h"
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
}

void StreamFetcher::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void StreamFetcher::run() {
    int chn_val = static_cast<int>(channel_);
    int fd = ss_mpi_venc_get_fd(chn_val);
    if (fd < 0) {
        return;
    }

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
            continue;
        }

        if (stat.cur_packs == 0) {
            continue;
        }

        ot_venc_stream stream;
        stream.pack = (ot_venc_pack*)malloc(sizeof(ot_venc_pack) * stat.cur_packs);
        if (stream.pack == nullptr) {
            continue;
        }
        stream.pack_cnt = stat.cur_packs;

        td_s32 ok = ss_mpi_venc_get_stream(chn_val, &stream, 1000);
        if (ok != 0) {
            free(stream.pack);
            continue;
        }

        auto frame = std::make_shared<StreamFrame>(channel_, stream_type_, codec_type_, stream);
        td_s32 release_ret = ss_mpi_venc_release_stream(chn_val, &stream);
        if (release_ret != TD_SUCCESS) {
        }
        free(stream.pack);
        stream.pack = nullptr;
        distributor_.push(frame);
    }
}
