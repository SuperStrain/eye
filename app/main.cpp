#include <iostream>
#include <vector>
#include <csignal>
#include <unistd.h>
#include "logger.h"
#include "hi_video_pipeline.h"
#include "stream_consumer_manager.h"
#include "hi_stream_fetcher.h"
#include "test.h"

void signal_handler(int sig) {
    std::cout << "Signal " << sig << " received" << std::endl;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);

    loggerSpace::Logger::instance().init();

    hiMppMedia::videoProcessHi::getInstance().init();

    auto& scm = StreamConsumerManager::instance();
    scm.set_fetcher(StreamType::VIDEO_MAIN,
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN0, StreamType::VIDEO_MAIN, CodecType::H265,
            scm.get_distributor(StreamType::VIDEO_MAIN))));
    scm.set_fetcher(StreamType::VIDEO_SUB,
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN1, StreamType::VIDEO_SUB, CodecType::H264,
            scm.get_distributor(StreamType::VIDEO_SUB))));
    scm.set_fetcher(StreamType::VIDEO_MJPEG,
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN2, StreamType::VIDEO_MJPEG, CodecType::MJPEG,
            scm.get_distributor(StreamType::VIDEO_MJPEG))));

    test_main();

    pause();

    std::cout << "eye exiting..." << std::endl;
    sleep(1);
    std::cout << "eye exiting gracefully." << std::endl;
    return 0;
}