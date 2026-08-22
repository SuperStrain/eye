#include "gst_stream_fetcher.h"
#include "gst_video_pipeline.h"
#include "logger.h"
#include "stream_frame.h"

#include <cstdio>
#include <pthread.h>
#include <unistd.h>

namespace rv1126bMedia {

namespace {

// 关键帧判定：无 DELTA_UNIT 标志即 IDR（h264parse/h265parse 已按 AU 输出）。
bool is_keyframe(GstBuffer* buffer) {
    return !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
}

} // namespace

GstStreamFetcher::GstStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                                   StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec), distributor_(distributor),
      app_sink_(GstVideoPipeline::getInstance().app_sink(chn)),
      seq_(0), frame_count_(0), bytes_total_(0), idr_count_(0),
      eos_reported_(false), running_(false) {}

GstStreamFetcher::~GstStreamFetcher() { stop(); }

int GstStreamFetcher::start() {
    if (running_) return 0;
    // 应用侧未显式调 IVideoPipeline::start()，由首个 fetcher 触发管线 PLAYING。
    int ret = GstVideoPipeline::getInstance().start();
    if (ret != 0) return ret;
    running_ = true;
    thread_ = std::thread(&GstStreamFetcher::run, this);
    return 0;
}

int GstStreamFetcher::stop() {
    if (!running_) return 0;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    return 0;
}

int GstStreamFetcher::fetchFrame(VencChannel chn, FrameData& frame) {
    (void)chn;
    if (!app_sink_) return -1;

    GstSample* sample = gst_app_sink_try_pull_sample(app_sink_, 100 * GST_MSECOND);
    if (!sample) {
        // 区分：普通 100ms 超时（静默）vs EOS（管线结束，首次告警避免刷屏）。
        if (!eos_reported_ && gst_app_sink_is_eos(app_sink_)) {
            eos_reported_ = true;
            LOGGER_WARN(STREAM, "GStreamer fetcher ch%d: appsink EOS (pipeline ended)",
                        static_cast<int>(channel_));
        }
        return -1;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return -1;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return -1;
    }

    frame.pack_count = 1;
    frame.seq = seq_++;
    frame.packs[0].data = map.data;
    frame.packs[0].len = static_cast<uint32_t>(map.size);
    if (codec_type_ == CodecType::MJPEG) {
        frame.packs[0].nalu_type = NaluType::IDR_SLICE;
    } else {
        frame.packs[0].nalu_type = is_keyframe(buffer) ? NaluType::IDR_SLICE : NaluType::P_SLICE;
    }

    // 统计：帧数/字节/关键帧（MJPEG 无 IDR 概念，不计关键帧）。
    const size_t frame_bytes = map.size;
    bool idr = false;
    if (codec_type_ != CodecType::MJPEG) {
        idr = (frame.packs[0].nalu_type == NaluType::IDR_SLICE);
        if (idr) idr_count_++;
    }
    frame_count_++;
    bytes_total_ += frame_bytes;

    // StreamFrame 构造时深拷贝；随后立即 unmap/unref，不把 GStreamer 缓冲指针带出构造。
    auto stream_frame = std::make_shared<StreamFrame>(channel_, stream_type_, codec_type_, frame);

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    distributor_.push(stream_frame);

    if (frame_count_ == 1) {
        const char* kind = (codec_type_ == CodecType::MJPEG) ? "MJPEG" : (idr ? "IDR" : "P");
        LOGGER_INFO(STREAM, "GStreamer fetcher ch%d: first frame %zu bytes (%s)",
                    static_cast<int>(channel_), frame_bytes, kind);
    } else if (frame_count_ % 100 == 0) {
        LOGGER_INFO(STREAM, "GStreamer fetcher ch%d: frames=%llu bytes=%llu idr=%llu",
                    static_cast<int>(channel_),
                    (unsigned long long)frame_count_,
                    (unsigned long long)bytes_total_,
                    (unsigned long long)idr_count_);
    }
    return 0;
}

int GstStreamFetcher::releaseFrame(VencChannel) {
    return 0;
}

void GstStreamFetcher::run() {
    char name[16];
    std::snprintf(name, sizeof(name), "GstFetch_%d", static_cast<int>(channel_));
    pthread_setname_np(pthread_self(), name);

    LOGGER_INFO(STREAM, "GStreamer fetcher ch%d running", static_cast<int>(channel_));
    while (running_) {
        FrameData frame;
        if (fetchFrame(channel_, frame) != 0) {
            usleep(10 * 1000);  // EOS/错误时 try_pull 立即返回，退避避免空转
        }
    }
    LOGGER_INFO(STREAM, "GStreamer fetcher ch%d stopped", static_cast<int>(channel_));
}

} // namespace rv1126bMedia
