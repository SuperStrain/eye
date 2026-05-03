#include "rtsp_server_media_subsession.h"
#include "rtsp_stream_source.h"
#include "logger.h"
#include <H264VideoRTPSink.hh>
#include <H265VideoRTPSink.hh>
#include <H264VideoStreamDiscreteFramer.hh>
#include <H265VideoStreamDiscreteFramer.hh>
#include <Base64.hh>

static std::string base64_encode(const std::vector<uint8_t>& data) {
    if (data.empty()) return "";
    char* b64 = base64Encode(
        reinterpret_cast<char*>(const_cast<uint8_t*>(data.data())),
        static_cast<unsigned>(data.size()));
    std::string result(b64);
    delete[] b64;
    return result;
}

RtspServerMediaSubsession* RtspServerMediaSubsession::createNew(
    UsageEnvironment& env, StreamType type, CodecType codec,
    std::shared_ptr<RtspFrameQueue> queue,
    std::shared_ptr<ParameterSetCache> param_cache,
    bool reuseFirstSource) {
    return new RtspServerMediaSubsession(
        env, type, codec, queue, param_cache, reuseFirstSource);
}

RtspServerMediaSubsession::RtspServerMediaSubsession(
    UsageEnvironment& env, StreamType type, CodecType codec,
    std::shared_ptr<RtspFrameQueue> queue,
    std::shared_ptr<ParameterSetCache> param_cache,
    bool reuseFirstSource)
    : OnDemandServerMediaSubsession(env, reuseFirstSource),
      stream_type_(type),
      codec_type_(codec),
      frame_queue_(queue),
      param_cache_(param_cache) {}

RtspServerMediaSubsession::~RtspServerMediaSubsession() {}

FramedSource* RtspServerMediaSubsession::createNewStreamSource(
    unsigned clientSessionId, unsigned& estBitrate) {
    (void)clientSessionId;

    RtspStreamSource* source = RtspStreamSource::createNew(
        envir(), stream_type_, codec_type_, frame_queue_);
    if (!source) return NULL;

    estBitrate = (codec_type_ == CodecType::H265) ? 4000 : 2000;

    if (codec_type_ == CodecType::H265) {
        return H265VideoStreamDiscreteFramer::createNew(envir(), source);
    } else {
        return H264VideoStreamDiscreteFramer::createNew(envir(), source);
    }
}

RTPSink* RtspServerMediaSubsession::createNewRTPSink(
    Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
    FramedSource* /*inputSource*/) {
    if (codec_type_ == CodecType::H265) {
        return H265VideoRTPSink::createNew(
            envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else {
        return H264VideoRTPSink::createNew(
            envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    }
}

char const* RtspServerMediaSubsession::getAuxSDPLine(
    RTPSink* rtpSink, FramedSource* /*inputSource*/) {
    if (!param_cache_) return NULL;

    std::lock_guard<std::mutex> lock(param_cache_->mutex);

    bool params_ready = (codec_type_ == CodecType::H265)
                            ? param_cache_->has_vps_sps_pps()
                            : param_cache_->has_sps_pps();

    if (!params_ready) {
        return NULL;
    }

    if (aux_sdp_line_.empty()) {
        if (codec_type_ == CodecType::H265) {
            std::string sprop_vps = base64_encode(param_cache_->vps);
            std::string sprop_sps = base64_encode(param_cache_->sps);
            std::string sprop_pps = base64_encode(param_cache_->pps);
            char* sdp_line = rtpSink->rtpmapLine();
            if (sdp_line) {
                aux_sdp_line_ = std::string(sdp_line) +
                    "a=fmtp:" + std::to_string(rtpSink->rtpPayloadType()) +
                    " sprop-vps=" + sprop_vps +
                    ";sprop-sps=" + sprop_sps +
                    ";sprop-pps=" + sprop_pps + "\r\n";
                delete[] sdp_line;
            }
        } else {
            std::string sprop_sps = base64_encode(param_cache_->sps);
            std::string sprop_pps = base64_encode(param_cache_->pps);
            char* sdp_line = rtpSink->rtpmapLine();
            if (sdp_line) {
                aux_sdp_line_ = std::string(sdp_line) +
                    "a=fmtp:" + std::to_string(rtpSink->rtpPayloadType()) +
                    " sprop-parameter-sets=" + sprop_sps +
                    "," + sprop_pps + "\r\n";
                delete[] sdp_line;
            }
        }
    }

    return aux_sdp_line_.c_str();
}
