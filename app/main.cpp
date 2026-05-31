#include <iostream>
#include <csignal>
#include <unistd.h>
#include "logger.h"
#include "hi_video_pipeline.h"
#include "stream_consumer_manager.h"
#include "hi_stream_fetcher.h"
#include "rtsp_server.h"
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

    auto& rtsp = RtspServer::instance();
    if (rtsp.start(8554)) {
        rtsp.add_stream(StreamType::VIDEO_MAIN, "main");
        rtsp.add_stream(StreamType::VIDEO_SUB, "sub");
        rtsp.set_overflow_callback(StreamType::VIDEO_MAIN, []() {
            hiMppMedia::videoProcessHi::getInstance().requestIdr(
                static_cast<int>(VencChannel::CHN0), true);
        });

        ConsumerConfig rtsp_config;
        rtsp_config.max_queue_size = 30;

        auto rtsp_cb = [](const StreamFrame& frame) {
            RtspServer::instance().on_frame(frame);
        };
        uint32_t main_rtsp_consumer =
            scm.register_consumer(StreamType::VIDEO_MAIN, rtsp_cb, rtsp_config);
        rtsp.set_main_consumer_stats_provider(
            [&scm, main_rtsp_consumer](ConsumerStats& stats) {
                return scm.get_distributor(StreamType::VIDEO_MAIN)
                    .get_consumer_stats(main_rtsp_consumer, stats);
            });
        scm.register_consumer(StreamType::VIDEO_SUB, rtsp_cb, rtsp_config);
    } else {
        LOGGER_ERROR(RTSP, "Failed to start RTSP server, streaming disabled");
    }

    test_main();

    pause();

    std::cout << "eye exiting..." << std::endl;
    sleep(1);
    std::cout << "eye exiting gracefully." << std::endl;
    return 0;
}
