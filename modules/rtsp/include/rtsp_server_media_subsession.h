#ifndef RTSP_SERVER_MEDIA_SUBSESSION_H
#define RTSP_SERVER_MEDIA_SUBSESSION_H

#include <liveMedia.hh>
#include <cstddef>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include "common_types.h"
#include "rtsp_frame_queue.h"
#include "rtsp_parameter_set_cache.h"

class RtspServerMediaSubsession : public OnDemandServerMediaSubsession {
public:
    static RtspServerMediaSubsession* createNew(
        UsageEnvironment& env,
        StreamType type,
        CodecType codec,
        std::shared_ptr<RtspFrameQueue> queue,
        std::shared_ptr<ParameterSetCache> param_cache,
        size_t max_clients,
        bool reuseFirstSource = true);

protected:
    virtual FramedSource* createNewStreamSource(
        unsigned clientSessionId, unsigned& estBitrate) override;
    virtual RTPSink* createNewRTPSink(
        Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
        FramedSource* inputSource) override;
    virtual char const* getAuxSDPLine(
        RTPSink* rtpSink, FramedSource* inputSource) override;
    virtual void deleteStream(unsigned clientSessionId,
                              void*& streamToken) override;

private:
    RtspServerMediaSubsession(
        UsageEnvironment& env,
        StreamType type,
        CodecType codec,
        std::shared_ptr<RtspFrameQueue> queue,
        std::shared_ptr<ParameterSetCache> param_cache,
        size_t max_clients,
        bool reuseFirstSource);
    virtual ~RtspServerMediaSubsession();

    StreamType stream_type_;
    CodecType codec_type_;
    std::shared_ptr<RtspFrameQueue> frame_queue_;
    std::shared_ptr<ParameterSetCache> param_cache_;
    std::string aux_sdp_line_;
    size_t max_clients_;
    std::set<unsigned> active_client_sessions_;
    std::mutex client_mutex_;
};

#endif
