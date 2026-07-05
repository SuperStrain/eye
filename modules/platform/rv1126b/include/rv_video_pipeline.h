#ifndef RV_VIDEO_PIPELINE_H
#define RV_VIDEO_PIPELINE_H

#include "i_video_encoder.h"
#include "i_video_pipeline.h"

namespace rv1126bMedia {

class RvVideoPipeline : public IVideoPipeline, public IVideoEncoder {
public:
    RvVideoPipeline(const RvVideoPipeline&) = delete;
    RvVideoPipeline& operator=(const RvVideoPipeline&) = delete;

    static RvVideoPipeline& getInstance();

    int init() override;
    int deinit() override;
    int start() override;
    int stop() override;

    int createChannel(int chn, CodecType codec, Size resolution) override;
    int destroyChannel(int chn) override;
    int startChannel(int chn) override;
    int stopChannel(int chn) override;

private:
    RvVideoPipeline();
    ~RvVideoPipeline();

    int init_sys();
    int init_vi_dev();
    int init_vi_channels();
    int init_venc_channels();
    int bind_channels();
    int unbind_channels();
    int deinit_venc_channels();
    int deinit_vi_channels();
    int deinit_vi_dev();

    bool initialized_;
    bool venc_created_[3] = {};
};

} // namespace rv1126bMedia

#endif
