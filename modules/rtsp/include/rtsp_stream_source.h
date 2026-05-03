#ifndef RTSP_STREAM_SOURCE_H
#define RTSP_STREAM_SOURCE_H

#include <liveMedia.hh>
#include <memory>
#include "rtsp_frame_queue.h"
#include "common_types.h"

class RtspStreamSource : public FramedSource {
public:
    static RtspStreamSource* createNew(UsageEnvironment& env,
                                       StreamType type,
                                       CodecType codec,
                                       std::shared_ptr<RtspFrameQueue> queue);

    void notify_frame_available();

protected:
    virtual void doGetNextFrame() override;
    virtual void doStopGettingFrames() override;

private:
    RtspStreamSource(UsageEnvironment& env,
                     StreamType type,
                     CodecType codec,
                     std::shared_ptr<RtspFrameQueue> queue);
    virtual ~RtspStreamSource();

    static void deliver_frame(RtspStreamSource* source);

    std::shared_ptr<RtspFrameQueue> frame_queue_;
    StreamType stream_type_;
    CodecType codec_type_;
    bool awaiting_frame_;
};

#endif
