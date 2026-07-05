#include <iostream>
#include <csignal>
#include <unistd.h>
#include "logger.h"
#include "platform_factory.h"
#include "stream_consumer_manager.h"
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

    platform_video_pipeline().init();

    auto& scm = StreamConsumerManager::instance();
    scm.set_fetcher(StreamType::VIDEO_MAIN,
        create_stream_fetcher(
            VencChannel::CHN0, StreamType::VIDEO_MAIN, CodecType::H265,
            scm.get_distributor(StreamType::VIDEO_MAIN)));
    scm.set_fetcher(StreamType::VIDEO_SUB,
        create_stream_fetcher(
            VencChannel::CHN1, StreamType::VIDEO_SUB, CodecType::H264,
            scm.get_distributor(StreamType::VIDEO_SUB)));
    scm.set_fetcher(StreamType::VIDEO_MJPEG,
        create_stream_fetcher(
            VencChannel::CHN2, StreamType::VIDEO_MJPEG, CodecType::MJPEG,
            scm.get_distributor(StreamType::VIDEO_MJPEG)));

    auto& rtsp = RtspServer::instance();
    if (rtsp.start(8554)) {
        rtsp.add_stream(StreamType::VIDEO_MAIN, "main");
        rtsp.add_stream(StreamType::VIDEO_SUB, "sub");

        ConsumerConfig rtsp_config;
        rtsp_config.max_queue_size = 5;

        auto rtsp_cb = [](const StreamFrame& frame) {
            RtspServer::instance().on_frame(frame);
        };
        scm.register_consumer(StreamType::VIDEO_MAIN, rtsp_cb, rtsp_config);
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