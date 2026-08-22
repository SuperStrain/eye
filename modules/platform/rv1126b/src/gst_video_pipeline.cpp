#include "gst_video_pipeline.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "logger.h"

namespace rv1126bMedia {

namespace {

// 与用户实测一致的 v4l2 源：/dev/video-camera0，NV16 3840x2160@30，io-mode=mmap。
constexpr char kDevice[] = "/dev/video-camera0";
constexpr char kRawFormat[] = "NV16";
constexpr int kMainWidth = 3840;
constexpr int kMainHeight = 2160;
constexpr int kSubWidth = 1280;
constexpr int kSubHeight = 720;
constexpr int kFrameRate = 30;
constexpr int kGop = 60;                  // kFrameRate * 2
constexpr int kMainBitrateBps = 8192000;  // 8192 kbps
constexpr int kSubBitrateBps = 2048000;   // 2048 kbps
constexpr int kMjpegFrameRate = 10;

constexpr char kAppSinkNames[][16] = {"app_main", "app_sub", "app_mjpeg"};

// live 源（v4l2src）的状态切换是异步的，等待其完成的上限。
const GstClockTime kStateChangeTimeout = 5 * GST_SECOND;

std::string build_pipeline_desc() {
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "v4l2src device=%s io-mode=mmap ! "
        "video/x-raw,format=%s,width=%d,height=%d,framerate=%d/1 ! tee name=t "
        "t. ! queue ! mpph265enc rc-mode=cbr bps=%d gop=%d ! "
        "h265parse ! video/x-h265,stream-format=byte-stream,alignment=au ! appsink name=app_main "
        "t. ! queue ! videoscale ! video/x-raw,width=%d,height=%d ! "
        "mpph264enc rc-mode=cbr bps=%d gop=%d ! "
        "h264parse ! video/x-h264,stream-format=byte-stream,alignment=au ! appsink name=app_sub "
        "t. ! queue ! videoscale ! video/x-raw,width=%d,height=%d ! "
        "videorate ! video/x-raw,framerate=%d/1 ! "
        "mppjpegenc ! appsink name=app_mjpeg",
        kDevice, kRawFormat, kMainWidth, kMainHeight, kFrameRate,
        kMainBitrateBps, kGop,
        kSubWidth, kSubHeight, kSubBitrateBps, kGop,
        kSubWidth, kSubHeight, kMjpegFrameRate);
    return std::string(buf);
}

} // namespace

GstVideoPipeline& GstVideoPipeline::getInstance() {
    static GstVideoPipeline instance;
    return instance;
}

GstVideoPipeline::GstVideoPipeline() : pipeline_(nullptr), initialized_(false) {
    std::memset(app_sinks_, 0, sizeof(app_sinks_));
}

GstVideoPipeline::~GstVideoPipeline() {}

int GstVideoPipeline::init() {
    if (initialized_) return 0;

    GError* error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &error)) {
        LOGGER_ERROR(GST, "gst_init_check failed: %s", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return -1;
    }

    std::string desc = build_pipeline_desc();
    LOGGER_INFO(GST, "pipeline: %s", desc.c_str());

    pipeline_ = gst_parse_launch(desc.c_str(), &error);
    if (!pipeline_) {
        LOGGER_ERROR(GST, "gst_parse_launch failed: %s", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return -1;
    }

    for (int i = 0; i < 3; ++i) {
        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), kAppSinkNames[i]);
        if (!sink) {
            LOGGER_ERROR(GST, "appsink %s not found", kAppSinkNames[i]);
            deinit();
            return -1;
        }
        app_sinks_[i] = GST_APP_SINK(sink);
        gst_object_unref(sink);  // 归还 get_by_name 的引用，pipeline 仍持有元素
    }

    // 到 PAUSED 验证所有元素可加载/可协商；真正 PLAYING 由 start() 触发（首个 fetcher 调用）。
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGGER_ERROR(GST, "pipeline failed to reach PAUSED");
        deinit();
        return -1;
    }

    GstState state;
    ret = gst_element_get_state(pipeline_, &state, nullptr, kStateChangeTimeout);
    if (ret == GST_STATE_CHANGE_FAILURE || ret == GST_STATE_CHANGE_ASYNC) {
        LOGGER_ERROR(GST, "pipeline failed to reach PAUSED (state change %d)", ret);
        deinit();
        return -1;
    }

    initialized_ = true;
    LOGGER_INFO(GST, "RV1126B GStreamer pipeline initialized");
    return 0;
}

int GstVideoPipeline::deinit() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    std::memset(app_sinks_, 0, sizeof(app_sinks_));
    initialized_ = false;
    return 0;
}

int GstVideoPipeline::start() {
    if (!initialized_ || !pipeline_) return -1;
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGGER_ERROR(GST, "pipeline failed to reach PLAYING");
        return -1;
    }

    GstState state;
    ret = gst_element_get_state(pipeline_, &state, nullptr, kStateChangeTimeout);
    if (ret == GST_STATE_CHANGE_FAILURE || ret == GST_STATE_CHANGE_ASYNC) {
        LOGGER_ERROR(GST, "pipeline failed to reach PLAYING (state change %d)", ret);
        return -1;
    }
    return 0;
}

int GstVideoPipeline::stop() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }
    return 0;
}

int GstVideoPipeline::createChannel(int, CodecType, Size) { return 0; }
int GstVideoPipeline::destroyChannel(int) { return 0; }
int GstVideoPipeline::startChannel(int) { return 0; }
int GstVideoPipeline::stopChannel(int) { return 0; }

GstAppSink* GstVideoPipeline::app_sink(VencChannel chn) {
    int idx = static_cast<int>(chn);
    if (idx < 0 || idx >= 3) return nullptr;
    return app_sinks_[idx];
}

} // namespace rv1126bMedia
