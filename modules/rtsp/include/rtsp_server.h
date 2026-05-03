#ifndef RTSP_SERVER_H
#define RTSP_SERVER_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <UsageEnvironment.hh>
#include "common_types.h"
#include "rtsp_frame_queue.h"
#include "rtsp_parameter_set_cache.h"
#include "stream_frame.h"

class RTSPServer;
class UsageEnvironment;
class TaskScheduler;
class ServerMediaSession;

struct RtspConfig {
    uint16_t port;
    size_t frame_queue_size;
    bool reuse_first_source;
    size_t max_clients;
};

class RtspServer {
public:
    static RtspServer& instance();

    bool start(uint16_t port = 8554);
    void stop();

    bool add_stream(StreamType type, const std::string& stream_name);
    void remove_stream(StreamType type);

    void on_frame(const StreamFrame& frame);

private:
    RtspServer();
    ~RtspServer();
    RtspServer(const RtspServer&) = delete;
    RtspServer& operator=(const RtspServer&) = delete;

    void event_loop();
    static void on_frame_trigger(void* clientData);
    void handle_frame_trigger();
    void process_frame_nals(const StreamFrame& frame);

    RTSPServer* rtsp_server_;
    UsageEnvironment* env_;
    TaskScheduler* scheduler_;
    std::thread event_thread_;
    std::atomic<bool> running_;
    EventLoopWatchVariable watch_variable_;

    std::map<StreamType, ServerMediaSession*> sessions_;
    std::map<StreamType, std::shared_ptr<RtspFrameQueue>> frame_queues_;
    std::map<StreamType, std::shared_ptr<ParameterSetCache>> parameter_sets_;
    std::map<StreamType, CodecType> stream_codecs_;
    EventTriggerId frame_event_trigger_;

    RtspConfig config_;
};

#endif
