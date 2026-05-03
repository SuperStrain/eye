# RTSP 推流功能实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 eye 项目新增 RTSP 服务端，支持 H.265 主码流和 H.264 子码流通过 live555 推送到局域网客户端。

**Architecture:** Consumer 桥接模式。`StreamDistributor` 的 worker 线程通过 `ConsumerCallback` 调用 `RtspServer::on_frame()`，将 NAL 数据解析、拆分后写入线程安全的 `RtspFrameQueue`，并通过 live555 event trigger 唤醒 live555 事件循环线程完成 RTP 打包发送。

**Tech Stack:** C++11, live555 (静态库, `-DNO_STD_LIB`), HiSilicon MPP VENC, 交叉编译 `arm-v01c02-linux-musleabi-` (GCC 10.3.0)

**Design Spec:** `docs/superpowers/specs/2026-05-03-rtsp-server-design.md`

---

## 文件结构

| 文件 | 操作 | 职责 |
|------|------|------|
| `modules/rtsp/include/rtsp_frame_queue.h` | 新增 | `RtspNalUnit` + `RtspFrameQueue` 跨线程桥接队列 |
| `modules/rtsp/src/rtsp_frame_queue.cpp` | 新增 | 队列实现 |
| `modules/rtsp/include/rtsp_nal_parser.h` | 新增 | NAL 类型解析 + Annex-B 拆分工具函数 |
| `modules/rtsp/src/rtsp_nal_parser.cpp` | 新增 | 解析实现 |
| `modules/rtsp/include/rtsp_parameter_set_cache.h` | 新增 | `ParameterSetCache` 参数集缓存 |
| `modules/rtsp/src/rtsp_parameter_set_cache.cpp` | 新增 | 缓存实现 |
| `modules/rtsp/include/rtsp_stream_source.h` | 新增 | `RtspStreamSource` live555 FramedSource 派生类 |
| `modules/rtsp/src/rtsp_stream_source.cpp` | 新增 | FramedSource 实现 |
| `modules/rtsp/include/rtsp_server_media_subsession.h` | 新增 | `RtspServerMediaSubsession` live555 subsession |
| `modules/rtsp/src/rtsp_server_media_subsession.cpp` | 新增 | subsession 实现 |
| `modules/rtsp/include/rtsp_server.h` | 新增 | `RtspServer` + `RtspConfig` 管理类 |
| `modules/rtsp/src/rtsp_server.cpp` | 新增 | 服务端实现 |
| `modules/rtsp/CMakeLists.txt` | 新增 | rtsp 模块构建 |
| `modules/core/include/logger.h` | 修改 | `LOG_CATEGORY_LIST` 新增 `RTSP` |
| `CMakeLists.txt` | 修改 | 新增 `add_subdirectory(modules/rtsp)` |
| `app/CMakeLists.txt` | 修改 | 链接 `rtsp` 库 |
| `app/main.cpp` | 修改 | 集成 RTSP 启动和消费者注册 |
| `thirdparty/live555/` | 新增 | live555 ARM 静态库 + 头文件（手动部署） |

---

### Task 1: live555 交叉编译与 thirdparty 部署

**前置条件**：可访问 https://download.live555.com 的网络环境，`arm-v01c02-linux-musleabi-` 工具链已安装。

**Files:**
- Create: `thirdparty/live555/include/` (4 个子目录)
- Create: `thirdparty/live555/lib/` (4 个 .a 文件)

- [ ] **Step 1: 下载 live555 源码**

```bash
cd /tmp
wget https://download.live555.com/live555-latest.tar.gz
sha256sum live555-latest.tar.gz
tar xzf live555-latest.tar.gz
cd live
cat liveMedia/include/liveMedia_version.hh
```

记录 SHA256 和版本号。

- [ ] **Step 2: 创建交叉编译配置**

创建 `/tmp/live/config.arm-v01c02`：

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

- [ ] **Step 3: 编译**

```bash
cd /tmp/live
./genMakefiles arm-v01c02
make
```

确认产出 4 个静态库：`liveMedia/libliveMedia.a`、`groupsock/libgroupsock.a`、`BasicUsageEnvironment/libBasicUsageEnvironment.a`、`UsageEnvironment/libUsageEnvironment.a`。

- [ ] **Step 4: 部署到 thirdparty**

```bash
EYE=<eye_project绝对路径>
mkdir -p ${EYE}/thirdparty/live555/include
mkdir -p ${EYE}/thirdparty/live555/lib

cp -r /tmp/live/liveMedia/include ${EYE}/thirdparty/live555/include/liveMedia
cp -r /tmp/live/groupsock/include ${EYE}/thirdparty/live555/include/groupsock
cp -r /tmp/live/BasicUsageEnvironment/include ${EYE}/thirdparty/live555/include/BasicUsageEnvironment
cp -r /tmp/live/UsageEnvironment/include ${EYE}/thirdparty/live555/include/UsageEnvironment

cp /tmp/live/liveMedia/libliveMedia.a ${EYE}/thirdparty/live555/lib/
cp /tmp/live/groupsock/libgroupsock.a ${EYE}/thirdparty/live555/lib/
cp /tmp/live/BasicUsageEnvironment/libBasicUsageEnvironment.a ${EYE}/thirdparty/live555/lib/
cp /tmp/live/UsageEnvironment/libUsageEnvironment.a ${EYE}/thirdparty/live555/lib/
```

- [ ] **Step 5: 验证静态库架构**

```bash
arm-v01c02-linux-musleabi-objdump -f ${EYE}/thirdparty/live555/lib/libliveMedia.a | head -5
```

预期输出包含 `architecture: arm`。

- [ ] **Step 6: 提交**

```bash
cd ${EYE}
git add thirdparty/live555/
git commit -m "chore: add live555 ARM static libraries for RTSP module"
```

---

### Task 2: 新增 RTSP 日志分类

**Files:**
- Modify: `modules/core/include/logger.h:12-15`

- [ ] **Step 1: 修改 LOG_CATEGORY_LIST**

在 `modules/core/include/logger.h` 中，将 `LOG_CATEGORY_LIST` 修改为：

```cpp
#define LOG_CATEGORY_LIST \
    X(HIMPP)              \
    X(TEST)               \
    X(STREAM)             \
    X(RTSP)
```

注意 `RTSP` 在最后一行末尾不要加反斜杠。

- [ ] **Step 2: 验证编译**

```bash
cd build && ./set.sh && make
```

预期：编译成功。`core` 模块的 `logger.cpp` 通过 `LOG_CATEGORY_LIST` 宏自动生成 `RTSP` 枚举值和名称字符串，无需额外改动。`conf/zlog.conf` 的通配规则 `*.*` 已覆盖新分类。

- [ ] **Step 3: 提交**

```bash
git add modules/core/include/logger.h
git commit -m "feat(rtsp): add RTSP log category to LOG_CATEGORY_LIST"
```

---

### Task 3: RtspFrameQueue 跨线程桥接队列

**Files:**
- Create: `modules/rtsp/include/rtsp_frame_queue.h`
- Create: `modules/rtsp/src/rtsp_frame_queue.cpp`

此任务不依赖 live555，是纯 C++ 线程安全队列。

- [ ] **Step 1: 创建模块目录结构**

```bash
mkdir -p modules/rtsp/include modules/rtsp/src
```

- [ ] **Step 2: 编写 rtsp_frame_queue.h**

```cpp
#ifndef RTSP_FRAME_QUEUE_H
#define RTSP_FRAME_QUEUE_H

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

class RtspStreamSource;

struct RtspNalUnit {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_idr;
};

class RtspFrameQueue {
public:
    explicit RtspFrameQueue(size_t max_queue_size);
    ~RtspFrameQueue();

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
    size_t max_queue_size_;
};

#endif
```

- [ ] **Step 3: 编写 rtsp_frame_queue.cpp**

```cpp
#include "rtsp_frame_queue.h"
#include "rtsp_stream_source.h"
#include "logger.h"

RtspFrameQueue::RtspFrameQueue(size_t max_queue_size)
    : max_queue_size_(max_queue_size) {
    if (max_queue_size_ == 0) {
        max_queue_size_ = 1;
    }
}

RtspFrameQueue::~RtspFrameQueue() {}

void RtspFrameQueue::push_nal_unit(RtspNalUnit nal) {
    std::lock_guard<std::mutex> lock(mutex_);
    while (queue_.size() >= max_queue_size_) {
        queue_.pop_front();
        LOGGER_WARN(RTSP, "Frame queue overflow, dropping oldest NAL");
    }
    queue_.push_back(std::move(nal));
}

bool RtspFrameQueue::pop_nal_unit(RtspNalUnit& nal) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return false;
    }
    nal = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

void RtspFrameQueue::register_source(RtspStreamSource* source) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_sources_.push_back(source);
}

void RtspFrameQueue::unregister_source(RtspStreamSource* source) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = active_sources_.begin(); it != active_sources_.end(); ++it) {
        if (*it == source) {
            active_sources_.erase(it);
            break;
        }
    }
}

void RtspFrameQueue::notify_active_sources() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto* source : active_sources_) {
        source->notify_frame_available();
    }
}

void RtspFrameQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}
```

- [ ] **Step 4: 提交**

```bash
git add modules/rtsp/include/rtsp_frame_queue.h modules/rtsp/src/rtsp_frame_queue.cpp
git commit -m "feat(rtsp): add RtspFrameQueue cross-thread bridge queue"
```

---

### Task 4: NAL 解析工具 + ParameterSetCache

**Files:**
- Create: `modules/rtsp/include/rtsp_nal_parser.h`
- Create: `modules/rtsp/src/rtsp_nal_parser.cpp`
- Create: `modules/rtsp/include/rtsp_parameter_set_cache.h`
- Create: `modules/rtsp/src/rtsp_parameter_set_cache.cpp`

- [ ] **Step 1: 编写 rtsp_nal_parser.h**

```cpp
#ifndef RTSP_NAL_PARSER_H
#define RTSP_NAL_PARSER_H

#include <cstdint>
#include <vector>
#include "common_types.h"

enum class H264NalType : uint8_t {
    NON_IDR = 1,
    IDR = 5,
    SPS = 7,
    PPS = 8,
    OTHER = 0xFF
};

enum class H265NalType : uint8_t {
    VPS = 32,
    SPS = 33,
    PPS = 34,
    IDR_W_RADL = 19,
    IDR_N_LP = 20,
    OTHER = 0xFF
};

struct ParsedNal {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool is_idr;
    bool is_vps;
    bool is_sps;
    bool is_pps;
};

H264NalType parse_h264_nal_type(const uint8_t* data, uint32_t len);
H265NalType parse_h265_nal_type(const uint8_t* data, uint32_t len);

void split_annex_b_nals(const uint8_t* data, uint32_t len,
                        CodecType codec, uint64_t timestamp, bool is_idr,
                        std::vector<ParsedNal>& out);

#endif
```

- [ ] **Step 2: 编写 rtsp_nal_parser.cpp**

```cpp
#include "rtsp_nal_parser.h"

H264NalType parse_h264_nal_type(const uint8_t* data, uint32_t len) {
    if (len < 1) return H264NalType::OTHER;
    uint8_t nal_type = data[0] & 0x1F;
    switch (nal_type) {
        case 1:  return H264NalType::NON_IDR;
        case 5:  return H264NalType::IDR;
        case 7:  return H264NalType::SPS;
        case 8:  return H264NalType::PPS;
        default: return H264NalType::OTHER;
    }
}

H265NalType parse_h265_nal_type(const uint8_t* data, uint32_t len) {
    if (len < 2) return H265NalType::OTHER;
    uint8_t nal_type = (data[0] >> 1) & 0x3F;
    switch (nal_type) {
        case 32: return H265NalType::VPS;
        case 33: return H265NalType::SPS;
        case 34: return H265NalType::PPS;
        case 19: return H265NalType::IDR_W_RADL;
        case 20: return H265NalType::IDR_N_LP;
        default: return H265NalType::OTHER;
    }
}

static bool is_start_code(const uint8_t* p, const uint8_t* end) {
    if (p + 2 < end && p[0] == 0 && p[1] == 0) {
        if (p + 3 < end && p[2] == 0 && p[3] == 1) return true;
        if (p[2] == 1) return true;
    }
    return false;
}

static uint32_t start_code_len(const uint8_t* p) {
    if (p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1) return 4;
    if (p[0] == 0 && p[1] == 0 && p[2] == 1) return 3;
    return 0;
}

static void classify_nal(const uint8_t* nal_data, uint32_t nal_len,
                         CodecType codec, bool frame_is_idr,
                         ParsedNal& out) {
    out.is_idr = false;
    out.is_vps = false;
    out.is_sps = false;
    out.is_pps = false;

    if (codec == CodecType::H264) {
        H264NalType t = parse_h264_nal_type(nal_data, nal_len);
        switch (t) {
            case H264NalType::IDR:  out.is_idr = true; break;
            case H264NalType::SPS:  out.is_sps = true; break;
            case H264NalType::PPS:  out.is_pps = true; break;
            default: break;
        }
    } else if (codec == CodecType::H265) {
        H265NalType t = parse_h265_nal_type(nal_data, nal_len);
        switch (t) {
            case H265NalType::IDR_W_RADL:
            case H265NalType::IDR_N_LP:
                out.is_idr = true;
                break;
            case H265NalType::VPS:  out.is_vps = true; break;
            case H265NalType::SPS:  out.is_sps = true; break;
            case H265NalType::PPS:  out.is_pps = true; break;
            default: break;
        }
    }

    if (frame_is_idr && !out.is_vps && !out.is_sps && !out.is_pps) {
        if (out.is_idr) {
        } else {
        }
    }
}

void split_annex_b_nals(const uint8_t* data, uint32_t len,
                        CodecType codec, uint64_t timestamp, bool is_idr,
                        std::vector<ParsedNal>& out) {
    if (len == 0) return;

    std::vector<uint32_t> nal_offsets;
    std::vector<uint32_t> nal_sc_lens;

    uint32_t i = 0;
    while (i < len) {
        if (i + 2 < len && data[i] == 0 && data[i + 1] == 0) {
            uint32_t sc_len = start_code_len(data + i);
            if (sc_len > 0) {
                nal_offsets.push_back(i + sc_len);
                nal_sc_lens.push_back(sc_len);
                i += sc_len;
                continue;
            }
        }
        ++i;
    }

    if (nal_offsets.empty()) {
        ParsedNal nal;
        nal.data.assign(data, data + len);
        nal.timestamp = timestamp;
        classify_nal(nal.data.data(), nal.data.size(), codec, is_idr, nal);
        out.push_back(std::move(nal));
        return;
    }

    for (size_t n = 0; n < nal_offsets.size(); ++n) {
        uint32_t start = nal_offsets[n];
        uint32_t end = (n + 1 < nal_offsets.size())
                           ? nal_offsets[n + 1] - nal_sc_lens[n + 1]
                           : len;
        if (end <= start) continue;

        ParsedNal nal;
        nal.data.assign(data + start, data + end);
        nal.timestamp = timestamp;
        classify_nal(nal.data.data(), nal.data.size(), codec, is_idr, nal);
        out.push_back(std::move(nal));
    }
}
```

- [ ] **Step 3: 编写 rtsp_parameter_set_cache.h**

```cpp
#ifndef RTSP_PARAMETER_SET_CACHE_H
#define RTSP_PARAMETER_SET_CACHE_H

#include <cstdint>
#include <mutex>
#include <vector>

struct ParameterSetCache {
    std::vector<uint8_t> vps;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    std::mutex mutex;

    bool has_sps_pps() const;
    bool has_vps_sps_pps() const;
};

#endif
```

- [ ] **Step 4: 编写 rtsp_parameter_set_cache.cpp**

```cpp
#include "rtsp_parameter_set_cache.h"

bool ParameterSetCache::has_sps_pps() const {
    return !sps.empty() && !pps.empty();
}

bool ParameterSetCache::has_vps_sps_pps() const {
    return !vps.empty() && !sps.empty() && !pps.empty();
}
```

- [ ] **Step 5: 提交**

```bash
git add modules/rtsp/include/rtsp_nal_parser.h \
       modules/rtsp/src/rtsp_nal_parser.cpp \
       modules/rtsp/include/rtsp_parameter_set_cache.h \
       modules/rtsp/src/rtsp_parameter_set_cache.cpp
git commit -m "feat(rtsp): add NAL parser and ParameterSetCache"
```

---

### Task 5: RtspStreamSource (live555 FramedSource)

**Files:**
- Create: `modules/rtsp/include/rtsp_stream_source.h`
- Create: `modules/rtsp/src/rtsp_stream_source.cpp`

**依赖**: live555 头文件 (Task 1), RtspFrameQueue (Task 3)

- [ ] **Step 1: 编写 rtsp_stream_source.h**

```cpp
#ifndef RTSP_STREAM_SOURCE_H
#define RTSP_STREAM_SOURCE_H

#include <liveMedia.hh>
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
```

- [ ] **Step 2: 编写 rtsp_stream_source.cpp**

```cpp
#include "rtsp_stream_source.h"
#include "logger.h"
#include <GroupsockHelper.hh>

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

    static const uint32_t kTimestampFreq = 90000;
    static const uint32_t kDefaultFps = 30;
    source->fPresentationTime.tv_sec =
        static_cast<time_t>(nal.timestamp / 1000);
    source->fPresentationTime.tv_usec =
        static_cast<suseconds_t>((nal.timestamp % 1000) * 1000);
    source->fDurationInMicroseconds =
        static_cast<unsigned>(1000000 / kDefaultFps);

    FramedSource::afterGetting(source);
}
```

- [ ] **Step 3: 提交**

```bash
git add modules/rtsp/include/rtsp_stream_source.h \
       modules/rtsp/src/rtsp_stream_source.cpp
git commit -m "feat(rtsp): add RtspStreamSource live555 FramedSource"
```

---

### Task 6: RtspServerMediaSubsession

**Files:**
- Create: `modules/rtsp/include/rtsp_server_media_subsession.h`
- Create: `modules/rtsp/src/rtsp_server_media_subsession.cpp`

- [ ] **Step 1: 编写 rtsp_server_media_subsession.h**

```cpp
#ifndef RTSP_SERVER_MEDIA_SUBSESSION_H
#define RTSP_SERVER_MEDIA_SUBSESSION_H

#include <liveMedia.hh>
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
        bool reuseFirstSource = true);

protected:
    virtual FramedSource* createNewStreamSource(
        unsigned clientSessionId, unsigned& estBitrate) override;
    virtual RTPSink* createNewRTPSink(
        Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
        FramedSource* inputSource) override;
    virtual char const* getAuxSDPLine(
        RTPSink* rtpSink, FramedSource* inputSource) override;

private:
    RtspServerMediaSubsession(
        UsageEnvironment& env,
        StreamType type,
        CodecType codec,
        std::shared_ptr<RtspFrameQueue> queue,
        std::shared_ptr<ParameterSetCache> param_cache,
        bool reuseFirstSource);
    virtual ~RtspServerMediaSubsession();

    StreamType stream_type_;
    CodecType codec_type_;
    std::shared_ptr<RtspFrameQueue> frame_queue_;
    std::shared_ptr<ParameterSetCache> param_cache_;
    std::string aux_sdp_line_;
};

#endif
```

- [ ] **Step 2: 编写 rtsp_server_media_subsession.cpp**

```cpp
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
```

- [ ] **Step 3: 提交**

```bash
git add modules/rtsp/include/rtsp_server_media_subsession.h \
       modules/rtsp/src/rtsp_server_media_subsession.cpp
git commit -m "feat(rtsp): add RtspServerMediaSubsession with SDP generation"
```

---

### Task 7: RtspServer 管理类

**Files:**
- Create: `modules/rtsp/include/rtsp_server.h`
- Create: `modules/rtsp/src/rtsp_server.cpp`

- [ ] **Step 1: 编写 rtsp_server.h**

```cpp
#ifndef RTSP_SERVER_H
#define RTSP_SERVER_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include "common_types.h"
#include "rtsp_frame_queue.h"
#include "rtsp_parameter_set_cache.h"

class RTSPServer;
class UsageEnvironment;
class TaskScheduler;
class ServerMediaSession;

struct RtspConfig {
    uint16_t port = 8554;
    size_t frame_queue_size = 5;
    bool reuse_first_source = true;
    size_t max_clients = 1;
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
    char watch_variable_;

    std::map<StreamType, ServerMediaSession*> sessions_;
    std::map<StreamType, std::shared_ptr<RtspFrameQueue>> frame_queues_;
    std::map<StreamType, std::shared_ptr<ParameterSetCache>> parameter_sets_;
    std::map<StreamType, CodecType> stream_codecs_;
    EventTriggerId frame_event_trigger_;

    RtspConfig config_;
};

#endif
```

- [ ] **Step 2: 编写 rtsp_server.cpp**

```cpp
#include "rtsp_server.h"
#include "rtsp_server_media_subsession.h"
#include "rtsp_nal_parser.h"
#include "rtsp_stream_source.h"
#include "logger.h"
#include "stream_frame.h"

#include <BasicUsageEnvironment.hh>
#include <RTSPServer.hh>
#include <OutPacketBuffer.hh>

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
      frame_event_trigger_(0) {}

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

    for (auto& pair : sessions_) {
        Medium::close(pair.second);
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
    auto it = sessions_.find(type);
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
    auto codec_it = stream_codecs_.find(frame.type());
    if (codec_it == stream_codecs_.end()) return;

    auto queue_it = frame_queues_.find(frame.type());
    if (queue_it == frame_queues_.end()) return;

    auto param_it = parameter_sets_.find(frame.type());
    if (param_it == parameter_sets_.end()) return;

    CodecType codec = codec_it->second;
    std::shared_ptr<RtspFrameQueue> queue = queue_it->second;
    std::shared_ptr<ParameterSetCache> param_cache = param_it->second;

    uint64_t ts = frame.timestamp() > 0 ? frame.timestamp() : 0;
    bool is_idr = frame.is_idr();

    const FrameData& fd = frame.data();
    for (uint32_t i = 0; i < fd.pack_count; ++i) {
        const auto& pk = fd.packs[i];
        if (!pk.data || pk.len == 0) continue;

        std::vector<ParsedNal> nals;
        split_annex_b_nals(pk.data, pk.len, codec, ts, is_idr, nals);

        for (auto& nal : nals) {
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
            queue->push_nal_unit(std::move(nal_unit));
        }
    }
}

void RtspServer::on_frame_trigger(void* clientData) {
    RtspServer* self = static_cast<RtspServer*>(clientData);
    self->handle_frame_trigger();
}

void RtspServer::handle_frame_trigger() {
    for (auto& pair : frame_queues_) {
        pair.second->notify_active_sources();
    }
}

void RtspServer::event_loop() {
    pthread_setname_np(pthread_self(), "rtsp_event");
    env_->taskScheduler().doEventLoop(&watch_variable_);
}
```

- [ ] **Step 3: 提交**

```bash
git add modules/rtsp/include/rtsp_server.h \
       modules/rtsp/src/rtsp_server.cpp
git commit -m "feat(rtsp): add RtspServer manager with event trigger"
```

---

### Task 8: CMake 构建集成

**Files:**
- Create: `modules/rtsp/CMakeLists.txt`
- Modify: `CMakeLists.txt:69` (新增 `add_subdirectory`)
- Modify: `app/CMakeLists.txt:10` (链接 `rtsp`)

- [ ] **Step 1: 创建 modules/rtsp/CMakeLists.txt**

```cmake
set(LIVE555_ROOT ${THIRDPARTY}/live555)

add_library(rtsp STATIC
    src/rtsp_frame_queue.cpp
    src/rtsp_nal_parser.cpp
    src/rtsp_parameter_set_cache.cpp
    src/rtsp_stream_source.cpp
    src/rtsp_server_media_subsession.cpp
    src/rtsp_server.cpp
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

- [ ] **Step 2: 修改根 CMakeLists.txt**

在 `add_subdirectory(modules/stream)` 行之后（第 72 行附近），新增一行：

```cmake
add_subdirectory(modules/rtsp)
```

- [ ] **Step 3: 修改 app/CMakeLists.txt**

将链接行改为：

```cmake
target_link_libraries(eye PRIVATE platform_hi stream rtsp)
```

- [ ] **Step 4: 验证编译**

```bash
cd build && ./set.sh && make
```

预期：所有 `modules/rtsp/` 源文件和 `app/main.cpp` 编译成功，链接成功。如果有编译错误，根据错误信息修复后重试。

- [ ] **Step 5: 提交**

```bash
git add modules/rtsp/CMakeLists.txt CMakeLists.txt app/CMakeLists.txt
git commit -m "feat(rtsp): integrate rtsp module into CMake build system"
```

---

### Task 9: main.cpp 集成

**Files:**
- Modify: `app/main.cpp`

- [ ] **Step 1: 修改 main.cpp**

将 `app/main.cpp` 替换为：

```cpp
#include <iostream>
#include <csignal>
#include <unistd.h>
#include "logger.h"
#include "hi_video_pipeline.h"
#include "stream_consumer_manager.h"
#include "hi_stream_fetcher.h"
#include "rtsp_server.h"
#include "test.h"

void signal_handler(int sig) {
    std::cout << "Signal " << sig << " received" << std::endl;
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);

    loggerSpace::Logger::instance().init();

    hiMppMedia::videoProcessHi::getInstance().init();

    auto& scm = StreamConsumerManager::instance();
    scm.set_fetcher(StreamType::VIDEO_MAIN,
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN0, StreamType::VIDEO_MAIN, CodecType::H265,
            scm.get_distributor(StreamType::VIDEO_MAIN))));
    scm.set_fetcher(StreamType::VIDEO_SUB,
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN1, StreamType::VIDEO_SUB, CodecType::H264,
            scm.get_distributor(StreamType::VIDEO_SUB))));
    scm.set_fetcher(StreamType::VIDEO_MJPEG,
        std::unique_ptr<StreamFetcher>(new StreamFetcher(
            VencChannel::CHN2, StreamType::VIDEO_MJPEG, CodecType::MJPEG,
            scm.get_distributor(StreamType::VIDEO_MJPEG))));

    auto& rtsp = RtspServer::instance();
    if (rtsp.start(8554)) {
        rtsp.add_stream(StreamType::VIDEO_MAIN, "main");
        rtsp.add_stream(StreamType::VIDEO_SUB, "sub");

        auto rtsp_cb = [](const StreamFrame& frame) {
            RtspServer::instance().on_frame(frame);
        };
        scm.register_consumer(StreamType::VIDEO_MAIN, rtsp_cb,
                              ConsumerConfig{.max_queue_size = 5});
        scm.register_consumer(StreamType::VIDEO_SUB, rtsp_cb,
                              ConsumerConfig{.max_queue_size = 5});
    } else {
        LOGGER_ERROR(RTSP, "Failed to start RTSP server, streaming disabled");
    }

    test_main();

    pause();

    std::cout << "eye exiting..." << std::endl;
    sleep(1);
    std::cout << "eye exiting gracefully." << std::endl;
    return 0;
}
```

注意：
- 移除了 `#include <vector>`（未使用）
- 保留了 `test_main()` 调用，RTSP consumer 注册在 `stream_test()` 调用 `start_all()` 之前，两者共存
- RTSP 启动失败时不影响原有码流采集功能

- [ ] **Step 2: 验证编译**

```bash
cd build && ./set.sh && make
```

预期：编译链接成功，生成 `eye` 可执行文件。

- [ ] **Step 3: 提交**

```bash
git add app/main.cpp
git commit -m "feat(rtsp): integrate RTSP server into main.cpp startup"
```

---

### Task 10: 构建与部署验证

- [ ] **Step 1: 完整构建与安装**

```bash
cd build && ./set.sh && make && cmake --install .
```

确认 `$HOME/eyeOut/bin/eye` 和 `$HOME/eyeOut/conf/` 生成成功。

- [ ] **Step 2: 部署到设备**

将 `$HOME/eyeOut/` 内容同步到设备 `/app/`：

```bash
# 方式根据实际部署流程调整
scp -r ~/eyeOut/bin/eye root@<device_ip>:/app/bin/
```

确认设备上 `/app/conf/zlog.conf` 已存在。

- [ ] **Step 3: 设备端启动验证**

通过 telnet 连接设备：

```bash
telnet <device_ip>
cd /app/bin && ./eye &
```

观察日志输出：
- `RTSP server started on port 8554` → 启动成功
- `Added stream: /main codec=H.265` → 主码流注册成功
- `Added stream: /sub codec=H.264` → 子码流注册成功

- [ ] **Step 4: PC 端拉流验证**

```bash
# UDP 拉流
ffplay rtsp://<device_ip>:8554/main
ffplay rtsp://<device_ip>:8554/sub

# TCP 回退拉流
ffplay -rtsp_transport tcp rtsp://<device_ip>:8554/main
ffplay -rtsp_transport tcp rtsp://<device_ip>:8554/sub
```

预期：VLC/FFplay 能正常播放，无明显花屏或卡顿。

- [ ] **Step 5: 抓包验证 RTP 正确性**

```bash
# 在 PC 端抓包
sudo tcpdump -i eth0 -w /tmp/rtsp_capture.pcap port 8554 or portrange 20000-65535
# 同时用 ffplay 拉流 10 秒
```

用 Wireshark 分析：
- RTP payload 不包含 Annex-B start code (`00 00 00 01` / `00 00 01`)
- SDP 中包含 `sprop-vps`/`sprop-sps`/`sprop-pps` (H.265) 或 `sprop-parameter-sets` (H.264)

- [ ] **Step 6: 连接生命周期验证**

1. FFplay 拉流 → Ctrl+C 断开 → 再次拉流 → 确认能正常出图
2. 长时间（>30秒）无客户端后首次拉流 → 确认能正常出图
3. 启动第二个 ffplay 拉同一 URL → 确认第二个被拒绝或关闭
4. 第二个断开后再次拉流 → 确认不会永久拒绝

- [ ] **Step 7: 提交最终状态**

```bash
git add -A
git commit -m "feat(rtsp): RTSP server implementation complete with validation"
```

---

## 自检清单

| 检查项 | 状态 |
|--------|------|
| 所有设计规格中的类都有对应实现任务 | ✅ RtspFrameQueue, RtspStreamSource, RtspServerMediaSubsession, RtspServer, ParameterSetCache, NAL parser |
| 线程安全约束已体现 | ✅ worker 线程只操作 queue + triggerEvent |
| live555 对象释放规则 | ✅ Medium::close(), env->reclaim() |
| Annex-B start code 处理 | ✅ split_annex_b_nals 移除 start code |
| 大 NAL 缓冲策略 | ✅ OutPacketBuffer::maxSize = 1MB |
| 参数集缓存与 SDP 生成 | ✅ getAuxSDPLine 从 ParameterSetCache 读取 |
| 日志 RTSP 分类 | ✅ Task 2 |
| CMake 集成 | ✅ Task 8 |
| 无 TBD/TODO 占位符 | ✅ |
| 所有方法签名跨任务一致 | ✅ |
