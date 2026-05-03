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
StreamFetcher → StreamDistributor → RtspConsumer(ConsumerCallback) → RtspServer 入队/解析参数集 → live555 event trigger → RtspStreamSource(live555 FramedSource) → RTPSink → 网络
```

优势：
- 完全复用现有消费者架构，与 `RecorderConsumer` 模式一致
- live555 事件循环与 `StreamDistributor` worker 线程通过队列和 event trigger 解耦，避免跨线程直接操作 live555 对象
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

    RTSPServer* rtsp_server_{nullptr};
    UsageEnvironment* env_{nullptr};
    TaskScheduler* scheduler_{nullptr};
    std::thread event_thread_;
    std::atomic<bool> running_{false};
    std::map<StreamType, ServerMediaSession*> sessions_;
    std::map<StreamType, std::shared_ptr<RtspFrameQueue>> frame_queues_;
    std::map<StreamType, std::shared_ptr<ParameterSetCache>> parameter_sets_;
    EventTriggerId frame_event_trigger_{0};
    RtspConfig config_;
};
```

- `start()`: 创建 `BasicTaskScheduler` + `UsageEnvironment`，启动 `RTSPServer`，在独立线程运行事件循环
- `add_stream()`: 创建 `ServerMediaSession` + `RtspServerMediaSubsession`，注册 RTSP URL
- `on_frame()`: 根据 `frame.type()` 解析/缓存参数集，拷贝 NAL 数据到对应 `RtspFrameQueue`，再触发 live555 event trigger，由 live555 事件循环线程完成投递
- `frame_event_trigger_`: `start()` 中通过 `TaskScheduler::createEventTrigger()` 创建；worker 线程只调用 `triggerEvent(frame_event_trigger_, this)`，trigger 回调在 live555 event loop 线程内遍历各路 `RtspFrameQueue::notify_active_sources()`

调用约束：
- 必须先调用 `start()` 初始化 live555 环境和 `RTSPServer`，再调用 `add_stream()` 注册 URL
- `add_stream()` 只在 `start()` 成功后调用；如果 `RTSPServer` 尚未创建，返回 `false` 并记录错误日志
- `stop()` 负责设置 watch variable 退出 live555 事件循环，等待 `event_thread_` 结束后再释放 `RTSPServer`、`UsageEnvironment`、`TaskScheduler`
- live555 对象按 live555 规则释放：`RTSPServer`/`ServerMediaSession` 使用 `Medium::close()`，`UsageEnvironment` 使用 `reclaim()`，不要用普通 `delete` 或无自定义 deleter 的 `unique_ptr`

RTSP URL：
- `rtsp://<device_ip>:8554/main` (H.265)
- `rtsp://<device_ip>:8554/sub` (H.264)

### RtspStreamSource

live555 `FramedSource` 派生类，桥接 `StreamDistributor` 与 live555。

```cpp
class RtspStreamSource : public FramedSource {
public:
    static RtspStreamSource* createNew(UsageEnvironment& env,
                                        StreamType type,
                                        CodecType codec,
                                        std::shared_ptr<RtspFrameQueue> queue);
    void notify_frame_available();

protected:
    virtual void doGetNextFrame() override;

private:
    RtspStreamSource(UsageEnvironment& env,
                     StreamType type,
                     CodecType codec,
                     std::shared_ptr<RtspFrameQueue> queue);
    static void deliver_frame(RtspStreamSource* source);

    std::shared_ptr<RtspFrameQueue> frame_queue_;
    StreamType stream_type_;
    CodecType codec_type_;
    bool awaiting_frame_{false};
};
```

- `RtspStreamSource` 不接收 `StreamDistributor` worker 线程直接调用；所有 live555 `FramedSource` 方法只在 live555 event loop 线程执行
- `notify_frame_available()`: 由 live555 event trigger 回调调用，如果当前处于 `awaiting_frame_` 状态则安排 `deliver_frame()`
- `doGetNextFrame()`: live555 事件循环调用，从共享 `RtspFrameQueue` 取一个待发送 NAL 并填充 `fTo`/`fFrameSize`
- `deliver_frame()`: 在 live555 事件循环线程内执行，必要时通过 `scheduleDelayedTask(0, ...)` 延迟投递，避免递归调用
- source 生命周期由 live555 管理；`RtspServer` 不保存 `RtspStreamSource*` 裸指针，避免客户端断连后悬空

### RtspFrameQueue

`RtspFrameQueue` 是跨线程桥接队列，归 `RtspServer` 持有并通过 `shared_ptr` 传给 subsession/source。

```cpp
struct RtspNalUnit {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_idr;
};

class RtspFrameQueue {
public:
    explicit RtspFrameQueue(size_t max_queue_size);
    void push_nal_unit(RtspNalUnit nal);
    bool pop_nal_unit(RtspNalUnit& nal);
    void register_source(RtspStreamSource* source);
    void unregister_source(RtspStreamSource* source);
    void notify_active_sources();
    void clear();

private:
    std::deque<RtspNalUnit> queue_;
    std::vector<RtspStreamSource*> active_sources_;
    std::mutex mutex_;
    size_t max_queue_size_{5};
};
```

- `push_nal_unit()`: 由 `RtspServer::on_frame()` 在 `StreamDistributor` worker 线程调用，队列满时丢弃最旧 NAL
- `pop_nal_unit()`: 只由 live555 event loop 线程调用
- `register_source()` / `unregister_source()`：只由 live555 event loop 线程调用，`RtspStreamSource` 构造/析构时注册/注销自身
- `notify_active_sources()`：只由 live555 event trigger 回调在 event loop 线程调用，遍历 `active_sources_` 并调用每个 source 的 `notify_frame_available()`
- 队列只保存已深拷贝后的编码数据，不保存上游 MPP buffer 指针
- 外部 worker 线程不能读写 `active_sources_`，避免 source 生命周期与跨线程访问交叉
- `max_queue_size_` 由 `RtspConfig::frame_queue_size` 创建时传入，不能在队列内部另行硬编码

`RtspNalUnit::data` 格式约束：
- 保存裸 NAL payload，不包含 Annex-B start code（`00 00 01` 或 `00 00 00 01`）
- H.264 裸 NAL 从 1 字节 NAL header 开始，H.265 裸 NAL 从 2 字节 NAL header 开始
- 从 Annex-B 字节流拆分时，start code 只作为分隔符，不能进入 `RtspNalUnit::data`
- 实现完成后必须用 FFplay/VLC 拉流和抓包确认 RTP payload 不包含 Annex-B start code；除非当前 live555 版本文档/源码明确要求保留 start code

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
    virtual char const* getAuxSDPLine(RTPSink* rtpSink,
                                      FramedSource* inputSource) override;

private:
    StreamType stream_type_;
    CodecType codec_type_;
    std::shared_ptr<RtspFrameQueue> frame_queue_;
    std::string aux_sdp_line_;
    std::shared_ptr<ParameterSetCache> parameter_sets_;
};
```

- `reuseFirstSource = true`: 复用首个 source；并发客户端仍需在 RTSP server 层限制为 1 个，拒绝第二个客户端或记录不支持
- `createNewRTPSink()`: H.265 返回 `H265VideoRTPSink`，H.264 返回 `H264VideoRTPSink`
- `createNewStreamSource()`: 返回对应的 `RtspStreamSource`，在前面加上 `H265VideoStreamDiscreteFramer` / `H264VideoStreamDiscreteFramer`
- `getAuxSDPLine()`: 为 H.264/H.265 生成 SDP 参数集信息。只读取 `ParameterSetCache` 中已缓存的 VPS/SPS/PPS，不从主播放队列消费数据；超时未获取关键帧时返回 `nullptr` 并记录错误

### ParameterSetCache

`ParameterSetCache` 保存 SDP 生成所需的 H.264/H.265 参数集，归 `RtspServer` 持有，并通过 `shared_ptr` 传给 subsession。

```cpp
struct ParameterSetCache {
    std::vector<uint8_t> vps; // H.265 only
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    std::mutex mutex;
};
```

- `RtspServer::on_frame()` 在把 NAL 入队前解析 H.264/H.265 NAL 类型，并更新该 stream 的参数集缓存
- `getAuxSDPLine()` 只复制缓存数据生成 SDP，不调用 `pop_nal_unit()`，避免 DESCRIBE 阶段消耗 PLAY 阶段需要发送的关键帧
- 如果缓存为空，`getAuxSDPLine()` 可以短时等待下一次 IDR/参数集到达；等待超时后返回失败，客户端可重试连接
- `RtspServer::add_stream()` 为每路 stream 创建一个 `ParameterSetCache`，生命周期覆盖对应 `ServerMediaSession` 和 `RtspServerMediaSubsession`

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
解析参数集 + RtspFrameQueue::push_nal_unit() [NAL 数据拷贝入队]
  ↓
live555 event trigger 唤醒事件循环线程
  ↓
live555 事件循环线程: RtspFrameQueue::notify_active_sources() → RtspStreamSource::notify_frame_available() → doGetNextFrame() → deliver_frame()
  ↓
RTPSink 打包 → UDP/TCP 网络发送
```

## 线程模型

| 线程 | 职责 |
|------|------|
| stream_0 | StreamDistributor CHN0 worker，调用 `RtspServer::on_frame()` 并写入 CHN0 `RtspFrameQueue` |
| stream_1 | StreamDistributor CHN1 worker，调用 `RtspServer::on_frame()` 并写入 CHN1 `RtspFrameQueue` |
| live555_event_loop | live555 事件循环，响应 event trigger，调用 `RtspFrameQueue::notify_active_sources()` / `doGetNextFrame()` / `RTPSink` 发送 |

- `StreamDistributor` worker 线程只允许操作 `RtspFrameQueue` 和触发 event trigger，不直接调用 `FramedSource`、`RTPSink`、`ServerMediaSession` 等 live555 对象
- `RtspFrameQueue` 内部通过 `mutex_` 保护队列，跨线程安全
- `RtspStreamSource` 的状态变量如 `awaiting_frame_` 只在 live555 event loop 线程读写，无需跨线程锁
- 每路码流使用独立的 `RtspFrameQueue`，CHN0/CHN1 互不竞争

## 帧数据打包

| 码流 | Codec | RTP Sink | NALU 处理 |
|------|-------|----------|----------|
| CHN0 | H.265 | `H265VideoRTPSink` | `H265VideoStreamDiscreteFramer` 按 NAL unit 边界输入 |
| CHN1 | H.264 | `H264VideoRTPSink` | `H264VideoStreamDiscreteFramer` 按 NAL unit 边界输入 |

`FrameData` 当前由多个 `FramePack` 组成，每个 pack 来自 HiSilicon VENC 的 `ot_venc_pack`。RTSP 模块不直接保存 `FramePack` 指针，而是在 `RtspServer::on_frame()` 中解析并拆分为 `RtspNalUnit`：

- 遍历 `frame.data().packs[0..pack_count)`，按顺序处理有效 `pack.data/pack.len`
- 如果 pack 已经对应单个完整 NAL，则直接深拷贝为一个 `RtspNalUnit`
- 如果 pack 中包含 Annex-B start code 且可能包含多个 NAL，则按 start code 拆分为多个 `RtspNalUnit`，并去掉 start code
- `RtspStreamSource::doGetNextFrame()` 每次只输出一个 `RtspNalUnit`，不把 SPS/PPS/IDR 等多个 NAL 拼成一次输出
- `H264VideoStreamDiscreteFramer` / `H265VideoStreamDiscreteFramer` 负责按 NAL unit 边界喂给 RTP sink，但不负责生成缺失的 VPS/SPS/PPS

大 NAL 缓冲策略：
- CHN0 H.265 2560x1440 的 IDR NAL 可能显著大于 live555 默认输出缓冲，RTSP 模块启动时需要设置 `OutPacketBuffer::maxSize`
- 建议初值：`OutPacketBuffer::maxSize = 1024 * 1024`，后续根据设备侧抓到的最大 IDR NAL 长度加余量调整
- `doGetNextFrame()` 必须检查 `RtspNalUnit::data.size()` 与 `fMaxSize`；如果超过 `fMaxSize`，设置 `fFrameSize = fMaxSize`、`fNumTruncatedBytes = data.size() - fMaxSize`，并限频记录错误日志
- 验证阶段必须观察是否出现 `fNumTruncatedBytes > 0` 或客户端关键帧花屏/黑屏；如果出现，应增大 `OutPacketBuffer::maxSize` 或降低编码瞬时峰值

参数集处理：
- H.265：首个可用 IDR 前后必须能解析出 VPS/SPS/PPS NAL，用于 SDP aux line 和后续客户端解码
- H.264：首个可用 IDR 前后必须能解析出 SPS/PPS NAL，用于 SDP aux line 和后续客户端解码
- 如果编码器不会在 IDR 中周期性携带参数集，需要在 VENC 配置阶段开启参数集随 IDR 输出，或在 RTSP 模块缓存最近一次参数集并在 IDR 前补发
- `DiscreteFramer` 不能替代参数集解析/缓存；实现时必须显式验证 VLC/FFplay 能在首次 DESCRIBE/PLAY 后正常解码

播放起始策略：
- 新客户端进入 PLAY 后不能从任意 P/B 帧开始发送，必须从可解码起点开始
- 推荐策略：客户端连接后清空该 source 的历史播放队列，等待下一次 IDR 及其参数集到达后再开始输出
- 如果实现最近关键帧缓存，也必须缓存完整的 IDR access unit 及对应 VPS/SPS/PPS（H.265）或 SPS/PPS（H.264），不能只缓存单个 IDR NAL
- 无客户端连接期间只更新 `ParameterSetCache`，不要求持续积累播放队列；新客户端连接时应避免发送旧的 P/B 帧
- 若等待 IDR 超时，允许保持连接并继续等待，或让客户端重试；必须限频记录 `LOGGER_WARN(RTSP, ...)`

参数集补发策略：
- RTSP 模块需要缓存每路最近一次参数集：H.265 缓存 VPS/SPS/PPS，H.264 缓存 SPS/PPS
- 如果 VENC 已配置为 IDR 周期性携带参数集，RTSP 只需解析并更新缓存，不需要额外补发
- 如果 VENC 不保证 IDR 携带参数集，RTSP 在每个 IDR access unit 前补发缓存的参数集 NAL，保证新客户端和丢帧恢复后能解码
- 参数集补发必须按 NAL unit 边界逐个送入 `DiscreteFramer`，不能把多个 NAL 拼成一个 `RtspNalUnit`
- 如果参数集缓存不完整，不应发送 IDR；应等待参数集齐全后再开始或恢复发送，并限频记录错误日志

## 错误处理

| 场景 | 处理方式 |
|------|----------|
| 端口绑定失败 | `start()` 返回 false，`LOGGER_ERROR`，RTSP 功能不可用但进程继续 |
| 无客户端连接 | 只更新参数集缓存，不积累播放队列 |
| 客户端断连 | live555 自动清理会话，清空对应 source 播放队列，等待新客户端从 IDR 开始 |
| 帧队列溢出 | 丢弃最旧帧，`LOGGER_WARN` 限频 |
| live555 内部错误 | `LOGGER_ERROR`，不影响码流采集 |
| 第二个客户端连接 | 拒绝连接或立即关闭会话，并 `LOGGER_WARN` 限频记录 |

## 配置

```cpp
struct RtspConfig {
    uint16_t port = 8554;
    size_t frame_queue_size = 5;
    bool reuse_first_source = true;
    size_t max_clients = 1;
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

需要同步修改：
- `modules/core/include/logger.h` 的 `LOG_CATEGORY_LIST` 增加 `X(RTSP)`
- `conf/zlog.conf` 当前使用通配规则，新增分类后无需单独 rule；如后续要按文件拆分 RTSP 日志，再补充 `RTSP.*` 专用规则

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
target_include_directories(rtsp PRIVATE
    ${LIVE555_ROOT}/include/liveMedia
    ${LIVE555_ROOT}/include/groupsock
    ${LIVE555_ROOT}/include/BasicUsageEnvironment
    ${LIVE555_ROOT}/include/UsageEnvironment
)
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

live555 常用头文件如 `liveMedia.hh`、`BasicUsageEnvironment.hh` 位于各组件 include 子目录内，因此 CMake 必须显式加入四个组件 include 路径，不能只加入 `${LIVE555_ROOT}/include`。

## 启动/关闭顺序

```
启动:
  1. loggerSpace::Logger::init()
  2. videoProcessHi::init()
  3. StreamFetcher 注册 (已有)
  4. RtspServer::start(8554)                          ← 新增，先创建 live555 环境和 RTSPServer
  5. RtspServer::add_stream(MAIN) + add_stream(SUB)   ← 新增，注册 /main 和 /sub
  6. register_consumer(MAIN/SUB, rtsp_cb)             ← 新增，consumer 回调只调用 RtspServer::on_frame()
  7. scm.start_all()

关闭 (后续 signal handler 扩展):
  1. scm.stop_all()
  2. unregister_consumer(MAIN/SUB)
  3. RtspServer::stop()
```

集成注意事项：
- 当前 `app/main.cpp` 会调用 `test_main()`，而 `test/stream/stream_test.cpp` 内部会注册文件写入 consumer 并调用 `StreamConsumerManager::start_all()`；正式集成 RTSP 时需要避免 `test_main()` 与 RTSP 启动逻辑重复启动 fetcher
- 如果保留 `test_main()` 作为临时验证入口，RTSP consumer 必须在 `stream_test()` 调用 `start_all()` 前注册；更推荐将正式启动逻辑移出 test，避免正式程序持续写 `/run/stream_*.h26x`
- `signal_handler` 后续真正支持退出时，应按关闭顺序停止 fetcher、注销 consumer、停止 RTSP event loop，再退出进程

单客户端限制实现：
- `reuseFirstSource=true` 只表示复用同一个输入 source，不等同于限制客户端数量
- `RtspServer` 需要维护 `active_client_count_`，在自定义 `RTSPServer`/`RTSPClientSession` 或 subsession 创建流之前检查 `max_clients`
- 当 `active_client_count_ >= max_clients` 时，拒绝第二个客户端或立即关闭会话，并限频记录 `LOGGER_WARN(RTSP, ...)`
- 客户端断连时必须递减 `active_client_count_`，避免异常断连后永久拒绝新客户端

## 改动文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `modules/rtsp/` | 新增 | 整个 RTSP 模块 |
| `thirdparty/live555/` | 新增 | live555 ARM 静态库 + 头文件 |
| `CMakeLists.txt` | 修改 | 新增 `add_subdirectory(modules/rtsp)` |
| `app/CMakeLists.txt` | 修改 | 链接 `rtsp` 库 |
| `app/main.cpp` | 修改 | 集成 RTSP 启动和消费者注册 |
| `modules/core/include/logger.h` | 修改 | `LOG_CATEGORY_LIST` 新增 `RTSP` 分类 |
| `conf/zlog.conf` | 可选修改 | 通配规则已覆盖 `RTSP`，仅在需要单独 RTSP 日志文件时修改 |

## 前置依赖

### live555 版本选择

- **版本来源**：使用 live555 官方发布包（https://download.live555.com/live555-latest.tar.gz），不要使用 GitHub 或系统软件源中的非官方副本
- **版本锁定**：`live555-latest.tar.gz` 只作为下载入口；实际引入 `thirdparty/live555/` 前必须记录已验证版本号（来自 `liveMedia/include/liveMedia_version.hh`）和 tarball SHA256，保证后续构建可复现
- **编译选项**：C++ 编译必须添加 `-DNO_STD_LIB` 以避免 `std::atomic_flag::test()` 等 C++20 标准库依赖
- **原因**：live555 自 2023.06.16 版本起可能使用 `std::atomic_flag::test()`（C++20 特性）。本项目使用 C++11（`CMAKE_CXX_STANDARD 11`），交叉编译器为 GCC 10.3.0。通过 `-DNO_STD_LIB` 使用 live555 的兼容实现，这是 live555 FAQ 针对该编译错误给出的方案
- **工具链**：`arm-v01c02-linux-musleabi-`（GCC 10.3.0, musl-1.2.3, 目标 ARM Cortex-A7）
- **版本升级策略**：后续升级 live555 时必须重新执行交叉编译、链接、VLC/FFplay 拉流和 RTP-over-TCP 回退验证，不能只替换头文件或静态库

### 交叉编译步骤

1. 下载并解压 live555 官方源码：
```bash
cd /tmp
wget https://download.live555.com/live555-latest.tar.gz
sha256sum live555-latest.tar.gz
tar xzf live555-latest.tar.gz
cd live
cat liveMedia/include/liveMedia_version.hh
```

将 SHA256 和 `liveMedia_version.hh` 中的版本号记录到提交说明或依赖清单中。

2. 创建交叉编译配置文件 `config.arm-v01c02`：
```
CROSS_COMPILE?=         arm-v01c02-linux-musleabi-
COMPILE_OPTS =          $(INCLUDES) -I. -O2 -DSOCKLEN_T=socklen_t -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64
C_COMPILER =            $(CROSS_COMPILE)gcc
C_FLAGS =               $(COMPILE_OPTS)
CPP =                   $(CROSS_COMPILE)cpp
CPLUSPLUS_COMPILER =    $(CROSS_COMPILE)g++
CPLUSPLUS_FLAGS =       $(COMPILE_OPTS) -Wall -DBSD=1 -DNO_STD_LIB
LINK =                  $(CROSS_COMPILE)g++ -o
LINK_OPTS =             -L.
CONSOLE_LINK_OPTS =     $(LINK_OPTS)
LIBRARY_LINK =          $(CROSS_COMPILE)ar cr
LIBRARY_LINK_OPTS =
LIB_SUFFIX =            a
LIBS_FOR_CONSOLE_APPLICATION =
LIBS_FOR_GUI_APPLICATION =
EXE =
```

3. 编译：
```bash
./genMakefiles arm-v01c02
make
```

4. 将产出物部署到项目 `thirdparty/`：
```bash
# 头文件
mkdir -p <eye_project>/thirdparty/live555/include
cp -r liveMedia/include <eye_project>/thirdparty/live555/include/liveMedia
cp -r groupsock/include <eye_project>/thirdparty/live555/include/groupsock
cp -r BasicUsageEnvironment/include <eye_project>/thirdparty/live555/include/BasicUsageEnvironment
cp -r UsageEnvironment/include <eye_project>/thirdparty/live555/include/UsageEnvironment

# 静态库
mkdir -p <eye_project>/thirdparty/live555/lib
cp liveMedia/libliveMedia.a <eye_project>/thirdparty/live555/lib/
cp groupsock/libgroupsock.a <eye_project>/thirdparty/live555/lib/
cp BasicUsageEnvironment/libBasicUsageEnvironment.a <eye_project>/thirdparty/live555/lib/
cp UsageEnvironment/libUsageEnvironment.a <eye_project>/thirdparty/live555/lib/
```

### 注意事项

- `-DNO_STD_LIB` 会启用 live555 的兼容实现。由于本设计有 CHN0/CHN1 两个 `StreamDistributor` worker 线程从 live555 外部触发事件，必须验证在该编译选项下 `TaskScheduler::triggerEvent()` 仍能安全处理多外部线程触发
- 保守实现要求：如果无法确认 `triggerEvent()` 在目标版本和 `-DNO_STD_LIB` 下的多线程安全性，则 RTSP 模块需要用单一桥接线程、pipe 或 eventfd 汇聚 CHN0/CHN1 帧事件，再由一个入口触发 live555 event loop
- live555 官方不维护旧版本，不建议使用 2023.06.16 之前的版本来回避 C++20 问题；应使用官方当前版本并锁定已验证版本号和 SHA256
- 当前验证版本下，eye 侧包含 live555 头文件应保持 C++11 可编译；后续升级 live555 时必须重新验证，不能假设头文件永远不引入更高标准依赖
- live555 使用 LGPL 许可证。本文档当前采用静态库链接（`libliveMedia.a` 等），产品分发前必须确认满足 LGPL 对用户替换/重新链接库的要求；如不能满足，应优先评估动态链接或提供可重新链接的目标文件/构建材料

## 验证项

基础拉流验证：
- 使用 FFplay 走默认 UDP 拉流验证两路码流：`ffplay rtsp://<device_ip>:8554/main`、`ffplay rtsp://<device_ip>:8554/sub`
- 使用 FFplay 强制 RTP-over-TCP 回退验证两路码流：`ffplay -rtsp_transport tcp rtsp://<device_ip>:8554/main`、`ffplay -rtsp_transport tcp rtsp://<device_ip>:8554/sub`
- 使用 VLC 分别拉取 `/main` 和 `/sub`，确认首屏能正常出图、持续播放无明显花屏/卡顿

连接生命周期验证：
- 客户端断开后重新连接，确认能再次从 IDR/参数集开始正常解码，而不是从旧 P/B 帧开始
- 长时间无客户端连接后再首次拉流，确认 RTSP 不发送过期播放队列中的旧帧，并能等待 IDR 后正常出图
- 同时启动第二个客户端，确认第二连接被拒绝或立即关闭，并且第一个客户端播放不受影响
- 第二个客户端断开后，再次拉取确认 `active_client_count_` 已正确递减，不会永久拒绝新客户端

码流正确性验证：
- 抓包确认 RTP payload 不包含 Annex-B start code，NAL 边界符合 `H264VideoStreamDiscreteFramer` / `H265VideoStreamDiscreteFramer` 输入要求
- 验证 IDR 前参数集补发策略：当 VENC 不周期性携带参数集时，新客户端仍能在首次 PLAY 后正常解码
- 观察日志和客户端表现，确认没有 `fNumTruncatedBytes > 0`、关键帧花屏或黑屏；如出现，调整 `OutPacketBuffer::maxSize` 或编码参数
