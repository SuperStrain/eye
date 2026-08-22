#ifndef GST_VIDEO_PIPELINE_H
#define GST_VIDEO_PIPELINE_H

#include "i_video_encoder.h"
#include "i_video_pipeline.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

namespace rv1126bMedia {

class GstVideoPipeline : public IVideoPipeline, public IVideoEncoder {
public:
    GstVideoPipeline(const GstVideoPipeline&) = delete;
    GstVideoPipeline& operator=(const GstVideoPipeline&) = delete;

    static GstVideoPipeline& getInstance();

    int init() override;
    int deinit() override;
    int start() override;
    int stop() override;

    int createChannel(int chn, CodecType codec, Size resolution) override;
    int destroyChannel(int chn) override;
    int startChannel(int chn) override;
    int stopChannel(int chn) override;

    GstAppSink* app_sink(VencChannel chn);

private:
    GstVideoPipeline();
    ~GstVideoPipeline();

    GstElement* pipeline_;
    GstAppSink* app_sinks_[3];
    bool initialized_;
};

} // namespace rv1126bMedia

#endif
