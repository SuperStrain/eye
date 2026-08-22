#include "gst_video_pipeline.h"

#include <cstring>

namespace rv1126bMedia {

GstVideoPipeline& GstVideoPipeline::getInstance() {
    static GstVideoPipeline instance;
    return instance;
}

GstVideoPipeline::GstVideoPipeline() : pipeline_(nullptr), initialized_(false) {
    std::memset(app_sinks_, 0, sizeof(app_sinks_));
}

GstVideoPipeline::~GstVideoPipeline() {}

int GstVideoPipeline::init() { return 0; }
int GstVideoPipeline::deinit() { return 0; }
int GstVideoPipeline::start() { return 0; }
int GstVideoPipeline::stop() { return 0; }
int GstVideoPipeline::createChannel(int, CodecType, Size) { return 0; }
int GstVideoPipeline::destroyChannel(int) { return 0; }
int GstVideoPipeline::startChannel(int) { return 0; }
int GstVideoPipeline::stopChannel(int) { return 0; }

GstAppSink* GstVideoPipeline::app_sink(VencChannel) { return nullptr; }

} // namespace rv1126bMedia
