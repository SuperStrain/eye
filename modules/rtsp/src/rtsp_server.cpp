#include "rtsp_server.h"
#include "rtsp_server_media_subsession.h"
#include "rtsp_nal_parser.h"
#include "rtsp_stream_source.h"
#include "logger.h"
#include "stream_frame.h"

#include <BasicUsageEnvironment.hh>
#include <MediaSink.hh>
#include <RTSPServer.hh>

RtspServer& RtspServer::instance() {
    static RtspServer inst;
    return inst;
}

RtspServer::RtspServer()
    : rtsp_server_(NULL),
      env_(NULL),
      scheduler_(NULL),
      running_(false),
      watch_variable_(0),
      frame_event_trigger_(0) {
    config_.port = 8554;
    config_.frame_queue_size = 30;
    config_.reuse_first_source = true;
    config_.max_clients = 1;
}

RtspServer::~RtspServer() {
    stop();
}

bool RtspServer::start(uint16_t port) {
    if (running_.load()) {
        LOGGER_WARN(RTSP, "RTSP server already running");
        return true;
    }

    config_.port = port;

    OutPacketBuffer::maxSize = 1024 * 1024;

    scheduler_ = BasicTaskScheduler::createNew();
    if (!scheduler_) {
        LOGGER_ERROR(RTSP, "Failed to create BasicTaskScheduler");
        return false;
    }

    env_ = BasicUsageEnvironment::createNew(*scheduler_);
    if (!env_) {
        LOGGER_ERROR(RTSP, "Failed to create UsageEnvironment");
        delete scheduler_;
        scheduler_ = NULL;
        return false;
    }

    rtsp_server_ = RTSPServer::createNew(*env_, port, NULL);
    if (!rtsp_server_) {
        LOGGER_ERROR(RTSP, "Failed to create RTSP server on port %u: %s",
                     port, env_->getResultMsg());
        env_->reclaim();
        env_ = NULL;
        delete scheduler_;
        scheduler_ = NULL;
        return false;
    }

    frame_event_trigger_ =
        scheduler_->createEventTrigger(on_frame_trigger);

    running_.store(true);
    watch_variable_ = 0;
    event_thread_ = std::thread(&RtspServer::event_loop, this);

    LOGGER_INFO(RTSP, "RTSP server started on port %u", port);
    return true;
}

void RtspServer::stop() {
    if (!running_.load()) return;

    watch_variable_ = 1;
    running_.store(false);

    if (event_thread_.joinable()) {
        event_thread_.join();
    }

    for (std::map<StreamType, ServerMediaSession*>::iterator it = sessions_.begin();
         it != sessions_.end(); ++it) {
        Medium::close(it->second);
    }
    sessions_.clear();

    frame_queues_.clear();
    parameter_sets_.clear();
    stream_codecs_.clear();

    if (rtsp_server_) {
        Medium::close(rtsp_server_);
        rtsp_server_ = NULL;
    }
    if (env_) {
        env_->reclaim();
        env_ = NULL;
    }
    if (scheduler_) {
        delete scheduler_;
        scheduler_ = NULL;
    }

    LOGGER_INFO(RTSP, "RTSP server stopped");
}

bool RtspServer::add_stream(StreamType type,
                            const std::string& stream_name) {
    if (!rtsp_server_) {
        LOGGER_ERROR(RTSP, "Cannot add stream: RTSP server not started");
        return false;
    }

    if (sessions_.find(type) != sessions_.end()) {
        LOGGER_WARN(RTSP, "Stream for type %d already exists",
                    static_cast<int>(type));
        return false;
    }

    CodecType codec = CodecType::H264;
    if (type == StreamType::VIDEO_MAIN) {
        codec = CodecType::H265;
    } else if (type == StreamType::VIDEO_SUB) {
        codec = CodecType::H264;
    } else {
        LOGGER_ERROR(RTSP, "Unsupported stream type %d",
                     static_cast<int>(type));
        return false;
    }

    std::shared_ptr<RtspFrameQueue> queue(
        new RtspFrameQueue(config_.frame_queue_size));
    std::shared_ptr<ParameterSetCache> cache(new ParameterSetCache());

    ServerMediaSession* sms = ServerMediaSession::createNew(
        *env_, stream_name.c_str(), stream_name.c_str(),
        "Live stream from eye");
    if (!sms) {
        LOGGER_ERROR(RTSP, "Failed to create ServerMediaSession for %s",
                     stream_name.c_str());
        return false;
    }

    RtspServerMediaSubsession* subsession =
        RtspServerMediaSubsession::createNew(
            *env_, type, codec, queue, cache,
            config_.max_clients,
            config_.reuse_first_source);
    sms->addSubsession(subsession);

    rtsp_server_->addServerMediaSession(sms);

    sessions_[type] = sms;
    frame_queues_[type] = queue;
    parameter_sets_[type] = cache;
    stream_codecs_[type] = codec;

    LOGGER_INFO(RTSP, "Added stream: /%s codec=%s",
                stream_name.c_str(),
                codec == CodecType::H265 ? "H.265" : "H.264");
    return true;
}

void RtspServer::remove_stream(StreamType type) {
    std::map<StreamType, ServerMediaSession*>::iterator it = sessions_.find(type);
    if (it == sessions_.end()) return;

    Medium::close(it->second);
    sessions_.erase(it);
    frame_queues_.erase(type);
    parameter_sets_.erase(type);
    stream_codecs_.erase(type);
}

void RtspServer::on_frame(const StreamFrame& frame) {
    if (!running_.load()) return;

    process_frame_nals(frame);

    if (scheduler_) {
        scheduler_->triggerEvent(frame_event_trigger_, this);
    }
}

void RtspServer::process_frame_nals(const StreamFrame& frame) {
    std::map<StreamType, CodecType>::iterator codec_it = stream_codecs_.find(frame.type());
    if (codec_it == stream_codecs_.end()) return;

    std::map<StreamType, std::shared_ptr<RtspFrameQueue>>::iterator queue_it =
        frame_queues_.find(frame.type());
    if (queue_it == frame_queues_.end()) return;

    std::map<StreamType, std::shared_ptr<ParameterSetCache>>::iterator param_it =
        parameter_sets_.find(frame.type());
    if (param_it == parameter_sets_.end()) return;

    CodecType codec = codec_it->second;
    std::shared_ptr<RtspFrameQueue> queue = queue_it->second;
    std::shared_ptr<ParameterSetCache> param_cache = param_it->second;

    uint64_t ts = frame.timestamp() > 0 ? frame.timestamp() : 0;
    bool is_idr = frame.is_idr();
    std::deque<RtspNalUnit> access_unit_nals;

    const FrameData& fd = frame.data();
    for (uint32_t i = 0; i < fd.pack_count; ++i) {
        const FramePack& pk = fd.packs[i];
        if (!pk.data || pk.len == 0) continue;

        std::vector<ParsedNal> nals;
        split_annex_b_nals(pk.data, pk.len, codec, ts, is_idr, nals);

        for (size_t j = 0; j < nals.size(); ++j) {
            ParsedNal& nal = nals[j];

            if (nal.is_vps || nal.is_sps || nal.is_pps) {
                std::lock_guard<std::mutex> lock(param_cache->mutex);
                if (nal.is_vps) param_cache->vps = nal.data;
                if (nal.is_sps) param_cache->sps = nal.data;
                if (nal.is_pps) param_cache->pps = nal.data;
            }

            RtspNalUnit nal_unit;
            nal_unit.data = std::move(nal.data);
            nal_unit.timestamp = ts;
            nal_unit.is_idr = nal.is_idr;
            access_unit_nals.push_back(std::move(nal_unit));
        }
    }

    if (queue->has_active_sources() && !access_unit_nals.empty()) {
        queue->push_access_unit(std::move(access_unit_nals));
    }
}

void RtspServer::on_frame_trigger(void* clientData) {
    RtspServer* self = static_cast<RtspServer*>(clientData);
    self->handle_frame_trigger();
}

void RtspServer::handle_frame_trigger() {
    for (std::map<StreamType, std::shared_ptr<RtspFrameQueue>>::iterator
             it = frame_queues_.begin(); it != frame_queues_.end(); ++it) {
        it->second->notify_active_sources();
    }
}

void RtspServer::event_loop() {
    pthread_setname_np(pthread_self(), "rtsp_event");
    env_->taskScheduler().doEventLoop(&watch_variable_);
}
