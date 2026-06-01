#include "rtsp_server.h"
#include "rtsp_server_media_subsession.h"
#include "rtsp_nal_parser.h"
#include "rtsp_monotonic_clock.h"
#include "rtsp_stream_source.h"
#include "logger.h"
#include "stream_distributor.h"
#include "stream_frame.h"

#include <BasicUsageEnvironment.hh>
#include <MediaSink.hh>
#include <RTSPServer.hh>

static const char* frame_type_name(NaluType type) {
    switch (type) {
        case NaluType::IDR_SLICE: return "IDR";
        case NaluType::P_SLICE: return "P";
        case NaluType::B_SLICE: return "B";
        default: return "OTHER";
    }
}

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
      frame_event_trigger_(0),
      main_stats_start_ms_(rtsp_monotonic_now_ms()),
      main_debug_log_time_ms_(main_stats_start_ms_),
      main_interval_frames_(0),
      main_interval_bytes_(0),
      main_interval_process_cost_us_(0),
      main_total_idr_frames_(0) {
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
    std::map<StreamType, std::function<void()>>::iterator cb_it =
        overflow_callbacks_.find(type);
    if (cb_it != overflow_callbacks_.end()) {
        queue->set_overflow_callback(cb_it->second);
    }
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
    overflow_callbacks_.erase(type);
}

void RtspServer::set_overflow_callback(StreamType type,
                                       std::function<void()> callback) {
    overflow_callbacks_[type] = callback;
    std::map<StreamType, std::shared_ptr<RtspFrameQueue>>::iterator it =
        frame_queues_.find(type);
    if (it != frame_queues_.end()) {
        it->second->set_overflow_callback(callback);
    }
}

void RtspServer::set_main_consumer_stats_provider(
    std::function<bool(ConsumerStats&)> provider) {
    main_consumer_stats_provider_ = std::move(provider);
}

void RtspServer::on_frame(const StreamFrame& frame) {
    if (!running_.load()) return;

    uint64_t start_us = rtsp_monotonic_now_us();
    size_t nal_count = process_frame_nals(frame);
    uint64_t process_cost_us = rtsp_monotonic_elapsed_us(
        start_us, rtsp_monotonic_now_us());

    if (frame.type() == StreamType::VIDEO_MAIN) {
        log_main_stream_stats(frame, nal_count, process_cost_us);
    }

    if (scheduler_) {
        scheduler_->triggerEvent(frame_event_trigger_, this);
    }
}

size_t RtspServer::process_frame_nals(const StreamFrame& frame) {
    std::map<StreamType, CodecType>::iterator codec_it = stream_codecs_.find(frame.type());
    if (codec_it == stream_codecs_.end()) return 0;

    std::map<StreamType, std::shared_ptr<RtspFrameQueue>>::iterator queue_it =
        frame_queues_.find(frame.type());
    if (queue_it == frame_queues_.end()) return 0;

    std::map<StreamType, std::shared_ptr<ParameterSetCache>>::iterator param_it =
        parameter_sets_.find(frame.type());
    if (param_it == parameter_sets_.end()) return 0;

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

    size_t nal_count = access_unit_nals.size();
    if (queue->has_active_sources() && !access_unit_nals.empty()) {
        queue->push_access_unit(std::move(access_unit_nals));
    }
    return nal_count;
}

void RtspServer::log_main_stream_stats(const StreamFrame& frame,
                                       size_t nal_count,
                                       uint64_t process_cost_us) {
    uint64_t now_ms = rtsp_monotonic_now_ms();
    size_t frame_size = frame.frame_size();
    NaluType frame_type = frame.frame_type();
    ++main_interval_frames_;
    main_interval_bytes_ += frame_size;
    main_interval_process_cost_us_ += process_cost_us;
    if (frame_type == NaluType::IDR_SLICE) {
        ++main_total_idr_frames_;
    }

    if (frame_type == NaluType::IDR_SLICE ||
        rtsp_monotonic_elapsed_ms(main_debug_log_time_ms_, now_ms) >= 1000) {
        LOGGER_DEBUG(RTSP,
            "RTSP main frame: frame_id=%llu frame_type=%s frame_size=%zu "
            "venc_pack_count=%u rtsp_nal_count=%zu rtsp_process_cost_us=%llu",
            static_cast<unsigned long long>(frame.frame_id()),
            frame_type_name(frame_type), frame_size, frame.data().pack_count,
            nal_count, static_cast<unsigned long long>(process_cost_us));
        main_debug_log_time_ms_ = now_ms;
    }

    int64_t elapsed_ms = rtsp_monotonic_elapsed_ms(main_stats_start_ms_, now_ms);
    if (elapsed_ms < 5000) return;

    std::map<StreamType, std::shared_ptr<RtspFrameQueue>>::iterator queue_it =
        frame_queues_.find(StreamType::VIDEO_MAIN);
    if (queue_it == frame_queues_.end()) return;
    RtspQueueStats queue_stats = queue_it->second->stats();

    ConsumerStats consumer_stats = {};
    bool has_consumer_stats = main_consumer_stats_provider_ &&
        main_consumer_stats_provider_(consumer_stats);
    double seconds_elapsed = elapsed_ms / 1000.0;
    double fps = main_interval_frames_ / seconds_elapsed;
    double bitrate_kbps = main_interval_bytes_ * 8.0 / seconds_elapsed / 1000.0;
    double avg_process_cost_us = main_interval_frames_ == 0 ? 0.0 :
        static_cast<double>(main_interval_process_cost_us_) / main_interval_frames_;

    LOGGER_INFO(RTSP,
        "RTSP main stats: frames=%llu fps=%.1f bitrate_kbps=%.1f "
        "last_frame_id=%llu last_frame_type=%s last_frame_size=%zu "
        "idr_total=%llu avg_process_cost_us=%.1f "
        "consumer_queue_depth=%zu/%zu consumer_queue_peak=%zu consumer_drop_total=%llu "
        "rtsp_queue_depth=%zu/%zu rtsp_queue_peak=%zu rtsp_drop_total=%llu rtsp_skip_total=%llu "
        "active_sources=%zu",
        static_cast<unsigned long long>(main_interval_frames_), fps, bitrate_kbps,
        static_cast<unsigned long long>(frame.frame_id()),
        frame_type_name(frame_type), frame_size,
        static_cast<unsigned long long>(main_total_idr_frames_),
        avg_process_cost_us,
        has_consumer_stats ? consumer_stats.queue_depth : 0,
        has_consumer_stats ? consumer_stats.max_queue_size : 0,
        has_consumer_stats ? consumer_stats.peak_queue_depth : 0,
        static_cast<unsigned long long>(
            has_consumer_stats ? consumer_stats.dropped : 0),
        queue_stats.queue_depth, queue_stats.max_queue_size,
        queue_stats.peak_queue_depth,
        static_cast<unsigned long long>(queue_stats.dropped),
        static_cast<unsigned long long>(queue_stats.skipped),
        queue_stats.active_sources);

    main_stats_start_ms_ = now_ms;
    main_interval_frames_ = 0;
    main_interval_bytes_ = 0;
    main_interval_process_cost_us_ = 0;
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
