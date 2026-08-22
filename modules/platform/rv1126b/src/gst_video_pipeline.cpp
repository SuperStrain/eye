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

// 运行期错误/警告/EOS/状态迁移经 bus 同步回调打到日志。本应用无 GMainLoop，
// 故用 gst_bus_set_sync_handler（在投递线程内同步执行）而非 gst_bus_add_watch。
GstBusSyncReply bus_sync_handler(GstBus*, GstMessage* msg, gpointer user_data) {
    GstElement* pipeline = static_cast<GstElement*>(user_data);
    const char* src = GST_MESSAGE_SRC(msg) ? GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)) : "(null)";

    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(msg, &err, &debug);
            LOGGER_ERROR(GST, "bus ERROR from %s: %s (%s)",
                         src, err ? err->message : "unknown", debug ? debug : "");
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_WARNING: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_warning(msg, &err, &debug);
            LOGGER_WARN(GST, "bus WARNING from %s: %s (%s)",
                        src, err ? err->message : "unknown", debug ? debug : "");
            g_error_free(err);
            g_free(debug);
            break;
        }
        case GST_MESSAGE_EOS: {
            LOGGER_WARN(GST, "bus EOS from %s", src);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            // 只打印顶层 pipeline 的状态迁移，避免每个元素刷屏。
            if (pipeline && GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                GstState old_state = GST_STATE_NULL;
                GstState new_state = GST_STATE_NULL;
                GstState pending = GST_STATE_NULL;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending);
                LOGGER_DEBUG(GST, "pipeline state: %s -> %s",
                             gst_element_state_get_name(old_state),
                             gst_element_state_get_name(new_state));
            }
            break;
        }
        default:
            break;
    }
    return GST_BUS_PASS;
}

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

    // 挂 bus 同步处理器：运行期错误/警告/EOS/状态迁移打到日志（便于设备侧定位）。
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    if (bus) {
        gst_bus_set_sync_handler(bus, bus_sync_handler, pipeline_, nullptr);
        gst_object_unref(bus);
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
        LOGGER_INFO(GST, "appsink %s acquired", kAppSinkNames[i]);
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
    LOGGER_INFO(GST, "pipeline reached %s", gst_element_state_get_name(state));
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
    LOGGER_INFO(GST, "RV1126B GStreamer pipeline deinitialized");
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
    LOGGER_INFO(GST, "pipeline reached PLAYING");
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
