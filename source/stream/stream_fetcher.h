#ifndef _STREAM_FETCHER_H_
#define _STREAM_FETCHER_H_

#include "stream_frame.h"
#include "stream_distributor.h"
#include <thread>
#include <atomic>

class StreamFetcher {
public:
    StreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                  StreamDistributor& distributor);
    ~StreamFetcher();

    void start();
    void stop();
    bool is_running() const { return running_; }

private:
    void run();

    VencChannel channel_;
    StreamType stream_type_;
    CodecType codec_type_;
    StreamDistributor& distributor_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

#endif
