#include "stream_test.h"
#include "stream_consumer_manager.h"
#include "stream_common.h"
#include "logger.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

struct ChannelWriter {
    FILE* fp;
    std::string path;
    std::string label;
    StreamType stream_type;
    std::atomic<uint64_t> frame_count;
    std::atomic<uint64_t> bytes_written;

    ChannelWriter(const char* p, const char* lbl, StreamType st)
        : fp(nullptr), path(p), label(lbl), stream_type(st),
          frame_count(0), bytes_written(0) {}

    bool open() {
        fp = fopen(path.c_str(), "wb");
        if (!fp) {
            int err = errno;
            LOGGER_ERROR(TEST, "fopen %s failed: %s", path.c_str(), strerror(err));
            return false;
        }
        LOGGER_INFO(TEST, "opened %s for writing", path.c_str());
        return true;
    }

    void close() {
        if (fp) {
            fflush(fp);
            fclose(fp);
            fp = nullptr;
        }
    }
};

static void frame_writer_cb(const StreamFrame& frame, ChannelWriter* writer) {
    const auto& fd = frame.data();
    for (uint32_t i = 0; i < fd.pack_count; ++i) {
        const auto& pk = fd.packs[i];
        size_t written = fwrite(pk.data, 1, pk.len, writer->fp);
        writer->bytes_written += written;
    }
    writer->frame_count++;
}

static const char* stream_type_to_string(StreamType type) {
    switch (type) {
        case StreamType::VIDEO_MAIN:  return "VIDEO_MAIN(CHN0 H265 2560x1440)";
        case StreamType::VIDEO_SUB:   return "VIDEO_SUB(CHN1 H264 1280x720)";
        case StreamType::VIDEO_MJPEG: return "VIDEO_MJPEG(CHN2 MJPEG 640x360)";
        default: return "UNKNOWN";
    }
}

void stream_test(int duration_sec, int delay_sec) {
    if (delay_sec > 0) {
        LOGGER_INFO(TEST, "=== stream_test: waiting %ds for ISP warmup ===", delay_sec);
        sleep(delay_sec);
    }
    LOGGER_INFO(TEST, "=== stream_test start, duration=%ds ===", duration_sec);

    std::unique_ptr<ChannelWriter> writers[] = {
        std::unique_ptr<ChannelWriter>(new ChannelWriter("/run/stream_chn0.h265",  "CHN0", StreamType::VIDEO_MAIN)),
        std::unique_ptr<ChannelWriter>(new ChannelWriter("/run/stream_chn1.h264",  "CHN1", StreamType::VIDEO_SUB)),
        std::unique_ptr<ChannelWriter>(new ChannelWriter("/run/stream_chn2.mjpeg", "CHN2", StreamType::VIDEO_MJPEG)),
    };

    uint32_t consumer_ids[3] = {0, 0, 0};
    bool reg_ok[3] = {false, false, false};

    for (int i = 0; i < 3; ++i) {
        if (!writers[i]->open()) {
            LOGGER_ERROR(TEST, "stream_test: failed to open writer for %s", writers[i]->label.c_str());
            goto cleanup;
        }
    }

    for (int i = 0; i < 3; ++i) {
        auto cb = std::bind(frame_writer_cb, std::placeholders::_1, writers[i].get());
        consumer_ids[i] = StreamConsumerManager::instance().register_consumer(
            writers[i]->stream_type, cb, ConsumerConfig{});
        reg_ok[i] = true;
        LOGGER_INFO(TEST, "stream_test: registered consumer for %s, id=%u",
                    writers[i]->label.c_str(), consumer_ids[i]);
    }

    StreamConsumerManager::instance().start_all();
    LOGGER_INFO(TEST, "stream_test: all fetchers started, recording %ds...", duration_sec);

    sleep(duration_sec);

    StreamConsumerManager::instance().stop_all();
    LOGGER_INFO(TEST, "stream_test: all fetchers stopped");

cleanup:
    for (int i = 0; i < 3; ++i) {
        if (reg_ok[i]) {
            StreamConsumerManager::instance().unregister_consumer(
                writers[i]->stream_type, consumer_ids[i]);
        }
        writers[i]->close();
    }

    LOGGER_INFO(TEST, "=== stream_test result ===");
    for (int i = 0; i < 3; ++i) {
        LOGGER_INFO(TEST, "  %s [%s]: frames=%llu bytes=%llu",
                    writers[i]->label.c_str(),
                    stream_type_to_string(writers[i]->stream_type),
                    (unsigned long long)writers[i]->frame_count.load(),
                    (unsigned long long)writers[i]->bytes_written.load());
    }
    LOGGER_INFO(TEST, "=== stream_test done ===");
}
