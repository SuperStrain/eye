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
      awaiting_frame_(false),
      nal_count_(0),
      first_presentation_time_(),
      first_frame_delivered_(false) {
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
        source->awaiting_frame_ = false;
        source->envir().taskScheduler().scheduleDelayedTask(
            1000, (TaskFunc*)deliver_frame, source);
        return;
    }

    if (nal.data.size() > source->fMaxSize) {
        LOGGER_ERROR(RTSP,
            "NAL too large, dropping: type=%d size=%zu max=%u",
            static_cast<int>(source->stream_type_),
            nal.data.size(), source->fMaxSize);
        source->envir().taskScheduler().scheduleDelayedTask(
            0, (TaskFunc*)deliver_frame, source);
        return;
    } else {
        source->fFrameSize = static_cast<unsigned>(nal.data.size());
        source->fNumTruncatedBytes = 0;
        memcpy(source->fTo, nal.data.data(), nal.data.size());
    }

    static const uint32_t kDefaultFps = 30;
    static const uint32_t kFrameDurationUs = 1000000 / kDefaultFps;

    if (!source->first_frame_delivered_) {
        gettimeofday(&source->first_presentation_time_, NULL);
        source->first_frame_delivered_ = true;
        source->nal_count_ = 0;
    }

    source->nal_count_++;
    uint64_t elapsed_us =
        static_cast<uint64_t>(source->nal_count_) * kFrameDurationUs;
    source->fPresentationTime.tv_sec =
        source->first_presentation_time_.tv_sec
        + static_cast<time_t>(elapsed_us / 1000000);
    source->fPresentationTime.tv_usec =
        source->first_presentation_time_.tv_usec
        + static_cast<suseconds_t>(elapsed_us % 1000000);
    if (source->fPresentationTime.tv_usec >= 1000000) {
        source->fPresentationTime.tv_sec +=
            source->fPresentationTime.tv_usec / 1000000;
        source->fPresentationTime.tv_usec %= 1000000;
    }
    source->fDurationInMicroseconds = kFrameDurationUs;

    FramedSource::afterGetting(source);
}
