# RTSP 推流功能设计规格

## 概述

为 eye 项目新增 RTSP 服务端功能，使设备能作为 RTSP 服务端，供局域网内客户端（VLC/FFplay 等）拉取实时视频流。

## 需求

| 项 | 值 |
|---|---|
| 推流通道 | CHN0 H.265 2560x1440 + CHN1 H.264 1280x720 |
| 协议角色 | RTSP 服务端（设备等待客户端拉流） |
| 并发客户端 | 1 个 |
| 网络环境 | 有线局域网 |
| RTP 传输 | UDP + TCP 回退 |
| 实现库 | live555 |

## 方案选择

**选定方案：Consumer 桥接模式**

```
StreamFetcher → StreamDistributor → RtspConsumer(ConsumerCallback) → RtspStreamSource(live555 FramedSource) → RTPSink → 网络
```

优势：
- 完全复用现有消费者架构，与 `RecorderConsumer` 模式一致
- live555 事件循环与 `StreamDistributor` worker 线程天然解耦
- 帧数据拷贝开销在有线局域网场景下可忽略

## 模块结构

```
modules/rtsp/                          ← 新增
├── CMakeLists.txt
├── include/
│   ├── rtsp_server.h
│   ├── rtsp_stream_source.h
│   └── rtsp_server_media_subsession.h
└── src/
    ├── rtsp_server.cpp
    ├── rtsp_stream_source.cpp
    └── rtsp_server_media_subsession.cpp

thirdparty/live555/                    ← 新增
├── include/
│   ├── liveMedia/
│   ├── groupsock/
│   ├── BasicUsageEnvironment/
│   └── UsageEnvironment/
└── lib/
    ├── libliveMedia.a
    ├── libgroupsock.a
    ├── libBasicUsageEnvironment.a
    └── libUsageEnvironment.a
```

## 核心类设计

### RtspServer

单例管理类，负责 live555 RTSP 服务生命周期。

```cpp
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
    void event_loop();

    std::unique_ptr<RTSPServer> rtsp_server_;
    UsageEnvironment* env_;
    TaskScheduler* scheduler_;
    std::thread event_thread_;
    std::atomic<bool> running_{false};
    std::map<StreamType, ServerMediaSession*> sessions_;
    std::map<StreamType, RtspStreamSource*> sources_;
};
```

- `start()`: 创建 `BasicTaskScheduler` + `UsageEnvironment`，启动 `RTSPServer`，在独立线程运行事件循环
- `add_stream()`: 创建 `ServerMediaSession` + `RtspServerMediaSubsession`，注册 RTSP URL
- `on_frame()`: 根据 `frame.type()` 分发到对应 `RtspStreamSource::on_frame()`

RTSP URL：
- `rtsp://<device_ip>:8554/main` (H.265)
- `rtsp://<device_ip>:8554/sub` (H.264)

### RtspStreamSource

live555 `FramedSource` 派生类，桥接 `StreamDistributor` 与 live555。

```cpp
class RtspStreamSource : public FramedSource {
public:
    static RtspStreamSource* createNew(UsageEnvironment& env,
                                        StreamType type, CodecType codec);
    void on_frame(const StreamFrame& frame);

protected:
    virtual void doGetNextFrame() override;

private:
    RtspStreamSource(UsageEnvironment& env, StreamType type, CodecType codec);
    static void deliver_frame(RtspStreamSource* source);

    std::deque<std::vector<uint8_t>> frame_queue_;
    std::mutex mutex_;
    StreamType stream_type_;
    CodecType codec_type_;
    bool awaiting_frame_{false};
    static constexpr size_t kMaxQueueSize = 5;
};
```

- `on_frame()`: 帧数据拷贝入队，队列满时丢弃最旧帧
- `doGetNextFrame()`: live555 事件循环调用，从队列取帧并填充 `fTo`/`fFrameSize`
- `deliver_frame()`: 通过 `scheduleDelayedTask` 延迟投递，避免阻塞事件循环

### RtspServerMediaSubsession

```cpp
class RtspServerMediaSubsession : public OnDemandServerMediaSubsession {
public:
    static RtspServerMediaSubsession* createNew(UsageEnvironment& env,
                                                 StreamType type,
                                                 CodecType codec,
                                                 bool reuseFirstSource = true);
protected:
    virtual FramedSource* createNewStreamSource(...) override;
    virtual RTPSink* createNewRTPSink(...) override;

private:
    StreamType stream_type_;
    CodecType codec_type_;
};
```

- `reuseFirstSource = true`: 单客户端模式
- `createNewRTPSink()`: H.265 返回 `H265VideoRTPSink`，H.264 返回 `H264VideoRTPSink`
- `createNewStreamSource()`: 返回对应的 `RtspStreamSource`，在前面加上 `H265VideoStreamDiscreteFramer` / `H264VideoStreamDiscreteFramer`

## 数据流

```
VENC 硬件
  ↓
StreamFetcher::fetchFrame()
  ↓
StreamDistributor::push()
  ↓
ConsumerSlot worker 线程 (stream_0 / stream_1)
  ↓
ConsumerCallback → RtspServer::on_frame()
  ↓
RtspStreamSource::on_frame() [帧数据拷贝入队]
  ↓
live555 事件循环线程: doGetNextFrame() → deliver_frame()
  ↓
RTPSink 打包 → UDP/TCP 网络发送
```

## 线程模型

| 线程 | 职责 |
|------|------|
| stream_0 | StreamDistributor CHN0 worker，调用 RtspStreamSource::on_frame() |
| stream_1 | StreamDistributor CHN1 worker，调用 RtspStreamSource::on_frame() |
| live555_event_loop | live555 事件循环，调用 doGetNextFrame() → RTPSink 发送 |

- 两个 `on_frame()` 各自操作独立的 `RtspStreamSource` 实例，无竞争
- `RtspStreamSource` 内部通过 `mutex_` 保护 `frame_queue_`，跨线程安全

## 帧数据打包

| 码流 | Codec | RTP Sink | NALU 处理 |
|------|-------|----------|----------|
| CHN0 | H.265 | `H265VideoRTPSink` | `H265VideoStreamDiscreteFramer` 自动处理 |
| CHN1 | H.264 | `H264VideoRTPSink` | `H264VideoStreamDiscreteFramer` 自动处理 |

- IDR 帧需携带 VPS+SPS+PPS（H.265）或 SPS+PPS（H.264），由 `DiscreteFramer` 自动处理
- 按 `FramePack` 逐个投喂到 `RtspStreamSource`

## 错误处理

| 场景 | 处理方式 |
|------|----------|
| 端口绑定失败 | `start()` 返回 false，`LOGGER_FATAL`，进程继续 |
| 无客户端连接 | 帧入队后丢弃，不积累内存 |
| 客户端断连 | live555 自动清理会话，帧继续入队覆盖 |
| 帧队列溢出 | 丢弃最旧帧，`LOGGER_WARN` 限频 |
| live555 内部错误 | `LOGGER_ERROR`，不影响码流采集 |

## 配置

```cpp
struct RtspConfig {
    uint16_t port = 8554;
    size_t frame_queue_size = 5;
    bool reuse_first_source = true;
};
```

硬编码默认值，后续可扩展为配置文件。

## 日志

新增日志分类 `RTSP`，使用现有日志宏：

```
LOGGER_INFO(RTSP, "RTSP server started on port %u", port);
LOGGER_WARN(RTSP, "Frame queue overflow for stream %d", type);
LOGGER_ERROR(RTSP, "Failed to start RTSP server: %s", msg);
```

## CMake 集成

### 根 CMakeLists.txt

新增 `add_subdirectory(modules/rtsp)`

### modules/rtsp/CMakeLists.txt

```cmake
set(LIVE555_ROOT ${THIRDPARTY}/live555)

add_library(rtsp STATIC
    src/rtsp_server.cpp
    src/rtsp_stream_source.cpp
    src/rtsp_server_media_subsession.cpp
)
target_include_directories(rtsp PUBLIC include)
target_include_directories(rtsp PRIVATE ${LIVE555_ROOT}/include)
target_link_libraries(rtsp PUBLIC
    stream
    ${LIVE555_ROOT}/lib/libliveMedia.a
    ${LIVE555_ROOT}/lib/libgroupsock.a
    ${LIVE555_ROOT}/lib/libBasicUsageEnvironment.a
    ${LIVE555_ROOT}/lib/libUsageEnvironment.a
)
```

### app/CMakeLists.txt

```cmake
target_link_libraries(eye PRIVATE platform_hi stream rtsp)
```

## 启动/关闭顺序

```
启动:
  1. loggerSpace::Logger::init()
  2. videoProcessHi::init()
  3. StreamFetcher 注册 (已有)
  4. RtspServer::add_stream(MAIN) + add_stream(SUB)  ← 新增
  5. RtspServer::start(8554)                          ← 新增
  6. register_consumer(MAIN/SUB, rtsp_cb)             ← 新增
  7. scm.start_all()

关闭 (后续 signal handler 扩展):
  1. RtspServer::stop()
  2. scm.stop_all()
```

## 改动文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `modules/rtsp/` | 新增 | 整个 RTSP 模块 |
| `thirdparty/live555/` | 新增 | live555 ARM 静态库 + 头文件 |
| `CMakeLists.txt` | 修改 | 新增 `add_subdirectory(modules/rtsp)` |
| `app/CMakeLists.txt` | 修改 | 链接 `rtsp` 库 |
| `app/main.cpp` | 修改 | 集成 RTSP 启动和消费者注册 |
| 日志配置 | 修改 | 新增 `RTSP` 分类 |

## 前置依赖

- live555 使用 `arm-v01c02-linux-musleabi-` 工具链交叉编译为 ARM 静态库
- live555 源码版本建议：live.2024.xx（稳定版）
