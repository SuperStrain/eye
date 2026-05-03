#include "rtsp_stream_source.h"
#include "logger.h"
#include <GroupsockHelper.hh>
#include <cstring>

RtspStreamSource* RtspStreamSource::createNew(
    UsageEnvironment& env, StreamType type, CodecType codec,
    std::shared_ptr<RtspFrameQueue> queue) {
    return new RtspStreamSource(env, type, codec, queue);
}

RtspStreamSource::RtspStreamSource(
    UsageEnvironment& env, StreamType type, CodecType codec,
    std::shared_ptr<RtspFrameQueue> queue)
    : FramedSource(env),
      frame_queue_(queue),
      stream_type_(type),
      codec_type_(codec),
      awaiting_frame_(false) {
    if (frame_queue_) {
        frame_queue_->register_source(this);
    }
}

RtspStreamSource::~RtspStreamSource() {
    if (frame_queue_) {
        frame_queue_->unregister_source(this);
    }
}

void RtspStreamSource::doGetNextFrame() {
    awaiting_frame_ = true;
    deliver_frame(this);
}

void RtspStreamSource::doStopGettingFrames() {
    awaiting_frame_ = false;
    FramedSource::doStopGettingFrames();
}

void RtspStreamSource::notify_frame_available() {
    if (!awaiting_frame_) return;
    awaiting_frame_ = false;
    envir().taskScheduler().scheduleDelayedTask(0,
        (TaskFunc*)deliver_frame, this);
}

void RtspStreamSource::deliver_frame(RtspStreamSource* source) {
    if (!source->isCurrentlyAwaitingData()) {
        source->awaiting_frame_ = false;
        return;
    }

    RtspNalUnit nal;
    if (!source->frame_queue_->pop_nal_unit(nal)) {
        source->awaiting_frame_ = true;
        return;
    }

    if (nal.data.size() > source->fMaxSize) {
        source->fNumTruncatedBytes =
            static_cast<unsigned>(nal.data.size() - source->fMaxSize);
        memcpy(source->fTo, nal.data.data(), source->fMaxSize);
        source->fFrameSize = source->fMaxSize;
        LOGGER_WARN(RTSP,
            "NAL truncated: type=%d size=%zu max=%u truncated=%u",
            static_cast<int>(source->stream_type_),
            nal.data.size(), source->fMaxSize,
            source->fNumTruncatedBytes);
    } else {
        source->fFrameSize = static_cast<unsigned>(nal.data.size());
        source->fNumTruncatedBytes = 0;
        memcpy(source->fTo, nal.data.data(), nal.data.size());
    }

    static const uint32_t kDefaultFps = 30;
    source->fPresentationTime.tv_sec =
        static_cast<time_t>(nal.timestamp / 1000);
    source->fPresentationTime.tv_usec =
        static_cast<suseconds_t>((nal.timestamp % 1000) * 1000);
    source->fDurationInMicroseconds =
        static_cast<unsigned>(1000000 / kDefaultFps);

    FramedSource::afterGetting(source);
}
