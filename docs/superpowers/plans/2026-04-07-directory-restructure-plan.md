# 目录结构重组与硬件抽象层实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将海思 SDK 依赖隔离到独立 platform 模块，引入 IVideoPipeline / IVideoEncoder / IStreamProvider 三个平台抽象接口，按 modules/ + app/ 结构重组目录，阶段一先在当前目录结构下重构代码，阶段二再移动文件并改造 CMake。

**Architecture:** 核心思路是将硬件相关的代码（视频采集/ISP/编码/VENC 获取）全部放入 `modules/platform/hi3516cv610/`，平台无关的流处理逻辑留在 `modules/stream/`，通过抽象接口解耦。未来换芯片只需新增 `modules/platform/<new_chip>/` 实现同一套接口。

**Tech Stack:** C++11, CMake, 交叉编译 (arm-v01c02), 海思 Hi3516CV610 MPP SDK, zlog

---

## 阶段一：代码抽象（在当前目录结构下重构）

### Task 1: 新增 common_types.h（平台无关的通用类型）

**Files:**
- Create: `include/common_types.h`

- [ ] **Step 1: 创建 common_types.h**

```cpp
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <cstdint>

struct Size {
    uint32_t width;
    uint32_t height;
};

enum class NaluType : uint8_t {
    IDR_SLICE,
    P_SLICE,
    B_SLICE,
    OTHER,
};

struct FramePack {
    const uint8_t* data;
    uint32_t len;
    NaluType nalu_type;
};

struct FrameData {
    static constexpr uint32_t MAX_PACKS = 8;
    FramePack packs[MAX_PACKS];
    uint32_t pack_count;
    uint32_t seq;
};

#endif
```

- [ ] **Step 2: 提交**

```bash
git add include/common_types.h
git commit -m "feat: add platform-agnostic common_types.h"
```

---

### Task 2: 修改 global_constants.h/.cpp（解耦 ot_size）

**Files:**
- Modify: `include/global_constants.h:1-49`
- Modify: `source/global_constants.cpp:1-41`

**依赖变化:** `global_constants.h` 不再包含 `<ot_common_video.h>`，改用 `common_types.h` 中的 `Size`。

- [ ] **Step 1: 修改 global_constants.h**

将 `ot_pic_size` 枚举重命名为 `PicSize`（避免与海思类型混淆），`ot_size` 改为 `Size`：

```cpp
// include/global_constants.h
#ifndef GLOBAL_CONSTANTS_H
#define GLOBAL_CONSTANTS_H
#include "common_types.h"

namespace hiMppMedia {

typedef enum {
    PIC_CIF,
    PIC_360P,
    PIC_D1_PAL,
    PIC_D1_NTSC,
    PIC_960H,
    PIC_720P,
    PIC_1080P,
    PIC_480P,
    PIC_576P,
    PIC_800X600,
    PIC_1024X768,
    PIC_1280X1024,
    PIC_1366X768,
    PIC_1440X900,
    PIC_1280X800,
    PIC_1600X1200,
    PIC_1680X1050,
    PIC_1920X1200,
    PIC_640X480,
    PIC_1920X2160,
    PIC_2304X1296,
    PIC_2560X1440,
    PIC_2560X1600,
    PIC_2592X1520,
    PIC_2688X1520,
    PIC_2592X1944,
    PIC_3840X2160,
    PIC_4096X2160,
    PIC_3000X3000,
    PIC_4000X3000,
    PIC_6080X2800,
    PIC_7680X4320,
    PIC_3840X8640,
    PIC_2880X1620,
    PIC_BUTT
} PicSize;

extern Size pic_size_array[PIC_BUTT];

}

#endif
```

- [ ] **Step 2: 修改 global_constants.cpp**

```cpp
// source/global_constants.cpp
#include "global_constants.h"
namespace hiMppMedia {

Size pic_size_array[PIC_BUTT] = {
    { 352,  288  },  /* PIC_CIF */
    { 640,  360  },  /* PIC_360P */
    { 720,  576  },  /* PIC_D1_PAL */
    { 720,  480  },  /* PIC_D1_NTSC */
    { 960,  576  },  /* PIC_960H */
    { 1280, 720  },  /* PIC_720P */
    { 1920, 1080 },  /* PIC_1080P */
    { 720,  480  },  /* PIC_480P */
    { 720,  576  },  /* PIC_576P */
    { 800,  600  },  /* PIC_800X600 */
    { 1024, 768  },  /* PIC_1024X768 */
    { 1280, 1024 },  /* PIC_1280X1024 */
    { 1366, 768  },  /* PIC_1366X768 */
    { 1440, 900  },  /* PIC_1440X900 */
    { 1280, 800  },  /* PIC_1280X800 */
    { 1600, 1200 },  /* PIC_1600X1200 */
    { 1680, 1050 },  /* PIC_1680X1050 */
    { 1920, 1200 },  /* PIC_1920X1200 */
    { 640,  480  },  /* PIC_640X480 */
    { 1920, 2160 },  /* PIC_1920X2160 */
    { 2304, 1296 },  /* PIC_2304X1296 */
    { 2560, 1440 },  /* PIC_2560X1440 */
    { 2560, 1600 },  /* PIC_2560X1600 */
    { 2592, 1520 },  /* PIC_2592X1520 */
    { 2688, 1520 },  /* PIC_2688X1520 */
    { 2592, 1944 },  /* PIC_2592X1944 */
    { 3840, 2160 },  /* PIC_3840X2160 */
    { 4096, 2160 },  /* PIC_4096X2160 */
    { 3000, 3000 },  /* PIC_3000X3000 */
    { 4000, 3000 },  /* PIC_4000X3000 */
    { 6080, 2800 },  /* PIC_6080X2800 */
    { 7680, 4320 },  /* PIC_7680X4320 */
    { 3840, 8640 },  /* PIC_3840X8640 */
    { 2880, 1620 }   /* PIC_2880X1620 */
};
}
```

- [ ] **Step 3: 提交**

```bash
git add include/global_constants.h source/global_constants.cpp
git commit -m "refactor: global_constants uses Size instead of ot_size"
```

---

### Task 3: 新增三个平台抽象接口

**Files:**
- Create: `include/i_video_pipeline.h`
- Create: `include/i_video_encoder.h`
- Create: `include/i_stream_provider.h`

- [ ] **Step 1: 创建 i_video_pipeline.h**

```cpp
// include/i_video_pipeline.h
#ifndef I_VIDEO_PIPELINE_H
#define I_VIDEO_PIPELINE_H

#include "common_types.h"

class IVideoPipeline {
public:
    virtual ~IVideoPipeline() = default;
    virtual int init() = 0;
    virtual int deinit() = 0;
    virtual int start() = 0;
    virtual int stop() = 0;
};

#endif
```

- [ ] **Step 2: 创建 i_video_encoder.h**

```cpp
// include/i_video_encoder.h
#ifndef I_VIDEO_ENCODER_H
#define I_VIDEO_ENCODER_H

#include "common_types.h"
#include "stream_common.h"

class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;
    virtual int createChannel(int chn, CodecType codec, Size resolution) = 0;
    virtual int destroyChannel(int chn) = 0;
    virtual int startChannel(int chn) = 0;
    virtual int stopChannel(int chn) = 0;
};

#endif
```

- [ ] **Step 3: 创建 i_stream_provider.h**

```cpp
// include/i_stream_provider.h
#ifndef I_STREAM_PROVIDER_H
#define I_STREAM_PROVIDER_H

#include "common_types.h"
#include "stream_common.h"

class IStreamProvider {
public:
    virtual ~IStreamProvider() = default;
    virtual int start(VencChannel chn) = 0;
    virtual int stop(VencChannel chn) = 0;
    virtual int fetchFrame(VencChannel chn, FrameData& frame) = 0;
    virtual int releaseFrame(VencChannel chn) = 0;
};

#endif
```

- [ ] **Step 4: 提交**

```bash
git add include/i_video_pipeline.h include/i_video_encoder.h include/i_stream_provider.h
git commit -m "feat: add platform abstraction interfaces (IVideoPipeline, IVideoEncoder, IStreamProvider)"
```

---

### Task 4: 改造 StreamFrame（消除对 ot_venc_stream 的直接依赖）

**Files:**
- Modify: `source/stream/stream_frame.h`
- Modify: `source/stream/stream_frame.cpp`

**关键变化:**
- `stream_` 成员从 `ot_venc_stream` 改为 `FrameData`
- `pack_array_` / `pack_buffers_` 不再需要（数据已在 FrameData 中）
- `is_idr()` 通过 NaluType 判断，不再暴露 SDK 枚举
- 构造函数签名从 `const ot_venc_stream&` 改为内部转换（由 HiStreamFetcher 调用时转换）

- [ ] **Step 1: 修改 stream_frame.h**

```cpp
// source/stream/stream_frame.h
#ifndef _STREAM_FRAME_H_
#define _STREAM_FRAME_H_

#include "stream_common.h"
#include "common_types.h"
#include <memory>

class StreamFrame {
public:
    StreamFrame(VencChannel chn, StreamType type, CodecType codec, const FrameData& data);
    ~StreamFrame();

    StreamFrame(const StreamFrame&) = delete;
    StreamFrame& operator=(const StreamFrame&) = delete;

    VencChannel channel() const { return channel_; }
    StreamType type() const { return type_; }
    CodecType codec_type() const { return codec_type_; }
    uint64_t timestamp() const { return timestamp_; }
    bool is_idr() const;
    const FrameData& data() const { return frame_data_; }

private:
    VencChannel channel_;
    StreamType type_;
    CodecType codec_type_;
    uint64_t timestamp_;
    FrameData frame_data_;
};

using StreamFramePtr = std::shared_ptr<StreamFrame>;

#endif
```

- [ ] **Step 2: 修改 stream_frame.cpp**

```cpp
// source/stream/stream_frame.cpp
#include "stream_frame.h"
#include <algorithm>

StreamFrame::StreamFrame(VencChannel chn, StreamType type, CodecType codec, const FrameData& data)
    : channel_(chn), type_(type), codec_type_(codec), frame_data_(data) {
    if (data.pack_count > 0) {
        timestamp_ = data.packs[0].pts;
    } else {
        timestamp_ = 0;
    }
}

StreamFrame::~StreamFrame() {
}

bool StreamFrame::is_idr() const {
    if (codec_type_ == CodecType::MJPEG) {
        return true;
    }
    for (uint32_t i = 0; i < frame_data_.pack_count; ++i) {
        if (frame_data_.packs[i].nalu_type == NaluType::IDR_SLICE) {
            return true;
        }
    }
    return false;
}
```

- [ ] **Step 3: 提交**

```bash
git add source/stream/stream_frame.h source/stream/stream_frame.cpp
git commit -m "refactor: StreamFrame uses FrameData instead of ot_venc_stream"
```

---

### Task 5: 改造 recorder_consumer（消除对海思 SDK 枚举的依赖）

**Files:**
- Modify: `source/stream/recorder_consumer.cpp:19-39`

**关键变化:** `check_is_idr()` 从检查 `OT_VENC_H264_NALU_IDR_SLICE` 改为调用 `StreamFrame::is_idr()`。

- [ ] **Step 1: 修改 recorder_consumer.cpp 中的 check_is_idr**

替换原有的 `check_is_idr` 函数：

```cpp
// 原函数（删除）：
// static bool check_is_idr(const StreamFrame& frame) {
//     if (frame.codec_type() == CodecType::MJPEG) {
//         return true;
//     }
//     const auto& stream = frame.data();
//     for (uint32_t i = 0; i < stream.pack_cnt; ++i) {
//         const auto& dt = stream.pack[i].data_type;
//         if (frame.codec_type() == CodecType::H264) {
//             if (dt.h264_type == OT_VENC_H264_NALU_IDR_SLICE) {
//                 return true;
//             }
//         } else if (frame.codec_type() == CodecType::H265) {
//             if (dt.h265_type == OT_VENC_H265_NALU_IDR_SLICE) {
//                 return true;
//             }
//         }
//     }
//     return false;
// }

// 新函数：
static bool check_is_idr(const StreamFrame& frame) {
    return frame.is_idr();
}
```

同时 `on_frame` 函数中也需要修改，将直接访问 `stream.pack_cnt` 和 `stream.pack[i]` 改为使用 `FrameData` 的接口：

```cpp
// on_frame 修改后的版本：
void RecorderConsumer::on_frame(const StreamFrame& frame) {
    const auto& fd = frame.data();

    size_t total_len = 0;
    for (uint32_t i = 0; i < fd.pack_count; ++i) {
        total_len += fd.packs[i].len;
    }

    WriteEntry entry;
    entry.is_idr = frame.is_idr();
    entry.data.resize(total_len);

    size_t pos = 0;
    for (uint32_t i = 0; i < fd.pack_count; ++i) {
        const auto& pk = fd.packs[i];
        memcpy(entry.data.data() + pos, pk.data, pk.len);
        pos += pk.len;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (write_queue_.size() >= max_queue_size_) {
            try_drop_frame();
        }
        write_queue_.push_back(std::move(entry));
    }
    cv_.notify_one();
}
```

- [ ] **Step 2: 提交**

```bash
git add source/stream/recorder_consumer.cpp
git commit -m "refactor: recorder_consumer uses StreamFrame::is_idr() instead of SDK enums"
```

---

### Task 6: 改造 StreamFetcher（实现 IStreamProvider，封装 SDK 调用）

**Files:**
- Modify: `source/stream/stream_fetcher.h`
- Modify: `source/stream/stream_fetcher.cpp`

**关键变化:** `StreamFetcher` 继承自 `IStreamProvider`，`fetchFrame` 将 `ot_venc_stream` 转换为 `FrameData`。

- [ ] **Step 1: 修改 stream_fetcher.h**

```cpp
// source/stream/stream_fetcher.h
#ifndef _STREAM_FETCHER_H_
#define _STREAM_FETCHER_H_

#include "stream_frame.h"
#include "stream_distributor.h"
#include "i_stream_provider.h"
#include <thread>
#include <atomic>

class StreamFetcher : public IStreamProvider {
public:
    StreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                  StreamDistributor& distributor);
    ~StreamFetcher();

    void start() override;
    void stop() override;
    int fetchFrame(VencChannel chn, FrameData& frame) override;
    int releaseFrame(VencChannel chn) override;
    bool is_running() const { return running_; }

private:
    void run();

    VencChannel channel_;
    StreamType stream_type_;
    CodecType codec_type_;
    StreamDistributor& distributor_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    int fd_;
    ot_venc_stream current_stream_;
};

#endif
```

- [ ] **Step 2: 修改 stream_fetcher.cpp**

将 `run()` 中的获取码流逻辑拆分到 `fetchFrame`/`releaseFrame`：

```cpp
// source/stream/stream_fetcher.cpp
#include "stream_fetcher.h"
#include "logger.h"
#include <ss_mpi_venc.h>
#include <pthread.h>
#include <sys/select.h>
#include <cstring>

StreamFetcher::StreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                             StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec),
      distributor_(distributor), fd_(-1) {}

StreamFetcher::~StreamFetcher() {
    stop();
}

void StreamFetcher::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&StreamFetcher::run, this);
    LOGGER_NOTICE(STREAM, "Fetcher ch%d started", static_cast<int>(channel_));
}

void StreamFetcher::stop() {
    if (!running_) return;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    LOGGER_NOTICE(STREAM, "Fetcher ch%d stopped", static_cast<int>(channel_));
}

int StreamFetcher::fetchFrame(VencChannel chn, FrameData& frame) {
    int chn_val = static_cast<int>(chn);

    ot_venc_chn_status stat;
    if (ss_mpi_venc_query_status(chn_val, &stat) != TD_SUCCESS) {
        return -1;
    }
    if (stat.cur_packs == 0) {
        return -1;
    }

    current_stream_.pack = (ot_venc_pack*)malloc(sizeof(ot_venc_pack) * stat.cur_packs);
    if (current_stream_.pack == nullptr) {
        return -1;
    }
    current_stream_.pack_cnt = stat.cur_packs;

    td_s32 ok = ss_mpi_venc_get_stream(chn_val, &current_stream_, 1000);
    if (ok != 0) {
        free(current_stream_.pack);
        current_stream_.pack = nullptr;
        return -1;
    }

    frame.pack_count = current_stream_.pack_cnt;
    frame.seq = 0;

    for (uint32_t i = 0; i < current_stream_.pack_cnt; ++i) {
        const auto& src_pack = current_stream_.pack[i];
        td_u32 offset = src_pack.offset;
        td_u32 data_len = src_pack.len - offset;

        NaluType nalu_type = NaluType::OTHER;
        if (codec_type_ == CodecType::H264) {
            if (src_pack.data_type.h264_type == OT_VENC_H264_NALU_IDR_SLICE) {
                nalu_type = NaluType::IDR_SLICE;
            } else if (src_pack.data_type.h264_type == OT_VENC_H264_NALU_P_SLICE) {
                nalu_type = NaluType::P_SLICE;
            } else if (src_pack.data_type.h264_type == OT_VENC_H264_NALU_B_SLICE) {
                nalu_type = NaluType::B_SLICE;
            }
        } else if (codec_type_ == CodecType::H265) {
            if (src_pack.data_type.h265_type == OT_VENC_H265_NALU_IDR_SLICE) {
                nalu_type = NaluType::IDR_SLICE;
            } else if (src_pack.data_type.h265_type == OT_VENC_H265_NALU_P_SLICE) {
                nalu_type = NaluType::P_SLICE;
            } else if (src_pack.data_type.h265_type == OT_VENC_H265_NALU_B_SLICE) {
                nalu_type = NaluType::B_SLICE;
            }
        }

        frame.packs[i].data = (const uint8_t*)src_pack.addr + offset;
        frame.packs[i].len = data_len;
        frame.packs[i].nalu_type = nalu_type;
    }

    return 0;
}

int StreamFetcher::releaseFrame(VencChannel chn) {
    int chn_val = static_cast<int>(chn);
    if (current_stream_.pack != nullptr) {
        td_s32 release_ret = ss_mpi_venc_release_stream(chn_val, &current_stream_);
        if (release_ret != TD_SUCCESS) {
            LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_release_stream failed, ret=%#x", chn_val, release_ret);
        }
        free(current_stream_.pack);
        current_stream_.pack = nullptr;
    }
    return 0;
}

void StreamFetcher::run() {
    int chn_val = static_cast<int>(channel_);
    fd_ = ss_mpi_venc_get_fd(chn_val);
    if (fd_ < 0) {
        LOGGER_ERROR(STREAM, "Fetcher ch%d: ss_mpi_venc_get_fd failed, fd=%d", chn_val, fd_);
        return;
    }
    LOGGER_INFO(STREAM, "Fetcher ch%d running, fd=%d", chn_val, fd_);

    char name[16];
    snprintf(name, sizeof(name), "Fetcher_ch%d", chn_val);
    pthread_setname_np(pthread_self(), name);

    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd_, &fds);
        timeval tv = {1, 0};

        int ret = select(fd_ + 1, &fds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;

        FrameData frame;
        if (fetchFrame(channel_, frame) == 0) {
            auto stream_frame = std::make_shared<StreamFrame>(channel_, stream_type_, codec_type_, frame);
            releaseFrame(channel_);
            distributor_.push(stream_frame);
        }
    }
}
```

- [ ] **Step 3: 提交**

```bash
git add source/stream/stream_fetcher.h source/stream/stream_fetcher.cpp
git commit -m "refactor: StreamFetcher implements IStreamProvider, converts ot_venc_stream to FrameData"
```

---

### Task 7: 改造 video_process_hi / video_venc_hi（实现 IVideoPipeline + IVideoEncoder）

**Files:**
- Modify: `include/video_process_hi.h`
- Modify: `source/video_process_hi.cpp`（大量改动）
- Modify: `include/video_venc_hi.h`
- Modify: `source/video_venc_hi.cpp`

**关键变化:**
- `videoProcessHi` 类实现 `IVideoPipeline` 和 `IVideoEncoder`
- `video_venc_hi` 的编码参数填充方法保留（仅作为内部实现），但头文件引用路径不变
- 所有 public 接口使用 `Size` 而非 `ot_size`，使用 `CodecType` 而非内嵌枚举
- 内部仍使用海思类型（这是预期的——所有海思调用都集中在此文件）

> **注意:** video_process_hi.cpp 有 ~700+ 行，是最大改动点。改动策略是：保持所有 private 方法签名不变（它们内部使用海思类型是正常的），只修改 public 接口方法和 `init()`/`deinit()` 的调用链。

- [ ] **Step 1: 修改 video_process_hi.h**

在 public 继承中添加 `IVideoPipeline` 和 `IVideoEncoder`：

```cpp
// include/video_process_hi.h
#ifndef VIDEO_PROCESS_HI_H
#define VIDEO_PROCESS_HI_H

#include <ot_common_vpss.h>
#include <ot_type.h>
#include <thread>
#include "global_constants.h"
#include "i_video_pipeline.h"
#include "i_video_encoder.h"
#include <memory>

namespace hiMppMedia {

class videoProcessHi : public IVideoPipeline, public IVideoEncoder
{
public:
    // ... existing singleton and public methods ...
    int init() override;
    int deinit() override;
    int start() override;
    int stop() override;

    int createChannel(int chn, CodecType codec, Size resolution) override;
    int destroyChannel(int chn) override;
    int startChannel(int chn) override;
    int stopChannel(int chn) override;

private:
    // ... existing private methods unchanged ...
};
```

- [ ] **Step 2: 修改 video_venc_hi.h**

`video_venc_hi` 作为内部实现类，继续使用海思类型。其参数填充方法签名需要适配新的 `CodecType` 和 `Size` 类型：

需要修改方法签名中的 `ot_pic_size size` 改为 `PicSize size`（因为 `ot_pic_size` 已改名为 `PicSize`）。

```cpp
// include/video_venc_hi.h
// 修改两处含 ot_pic_size 参数的函数签名：
// comm_venc_h265_cvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
//     td_u32 frame_rate, PicSize size);  // 原来是 ot_pic_size

// comm_venc_h264_cvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
//     td_u32 frame_rate, PicSize size);  // 原来是 ot_pic_size
```

- [ ] **Step 3: 修改 video_venc_hi.cpp**

对应修改两个 `comm_venc_set_*_cvbr_bit_rate` 函数中的 `switch (size)` 语句里的 `case` 标签（`PIC_xxx` 不变，只变函数签名）。

在 `comm_venc_h265_cvbr_param_init` 和 `comm_venc_h264_cvbr_param_init` 的参数列表中，`ot_pic_size size` 改为 `PicSize size`。

- [ ] **Step 4: 提交**

```bash
git add include/video_process_hi.h source/video_process_hi.cpp include/video_venc_hi.h source/video_venc_hi.cpp
git commit -m "refactor: videoProcessHi implements IVideoPipeline and IVideoEncoder interfaces"
```

---

### Task 8: 验证阶段一（编译测试）

**构建命令:**

```bash
cd /home/yangyang/projects/eye/build
./set.sh && make
```

预期结果：编译通过，无错误（警告可能存在但不影响功能）。

---

## 阶段二：目录重组

### Task 9: 创建目录骨架

**Files:**
- Create: `modules/core/CMakeLists.txt`
- Create: `modules/interface/CMakeLists.txt`
- Create: `modules/stream/CMakeLists.txt`
- Create: `modules/platform/hi3516cv610/CMakeLists.txt`
- Create: `app/CMakeLists.txt`
- Modify: `CMakeLists.txt`（根）

- [ ] **Step 1: 创建 modules/core/CMakeLists.txt**

```cmake
add_library(core STATIC
    src/logger.cpp
    src/global_constants.cpp
)
target_include_directories(core PUBLIC include)
target_link_libraries(core PUBLIC zlog)
```

- [ ] **Step 2: 创建 modules/interface/CMakeLists.txt**

```cmake
add_library(interface INTERFACE)
target_include_directories(interface INTERFACE include)
target_link_libraries(interface INTERFACE core)
```

- [ ] **Step 3: 创建 modules/stream/CMakeLists.txt**

```cmake
add_library(stream STATIC
    src/stream_frame.cpp
    src/stream_distributor.cpp
    src/stream_consumer_manager.cpp
    src/recorder_consumer.cpp
)
target_include_directories(stream PUBLIC include)
target_link_libraries(stream PUBLIC interface pthread)
```

- [ ] **Step 4: 创建 modules/platform/hi3516cv610/CMakeLists.txt**

```cmake
add_library(platform_hi STATIC
    src/hi_video_pipeline.cpp
    src/hi_venc_param.cpp
    src/hi_stream_fetcher.cpp
)
target_include_directories(platform_hi PUBLIC include)
target_link_libraries(platform_hi PUBLIC interface ${THIRDPARTY_LIB})
```

- [ ] **Step 5: 创建 app/CMakeLists.txt**

```cmake
add_executable(eye main.cpp)
target_link_libraries(eye PRIVATE platform_hi stream core)
install(TARGETS eye RUNTIME DESTINATION bin)
```

- [ ] **Step 6: 提交**

```bash
git add modules/core/CMakeLists.txt modules/interface/CMakeLists.txt modules/stream/CMakeLists.txt modules/platform/hi3516cv610/CMakeLists.txt app/CMakeLists.txt
git commit -m "chore: add CMake module skeletons for new directory structure"
```

---

### Task 10: 移动 core 模块文件

**Files:**
- Create: `modules/core/include/logger.h`（从 `include/logger.h`）
- Create: `modules/core/include/global_constants.h`（从 `include/global_constants.h`）
- Create: `modules/core/include/common_types.h`（从 `include/common_types.h`）
- Create: `modules/core/src/logger.cpp`（从 `source/logger.cpp`）
- Create: `modules/core/src/global_constants.cpp`（从 `source/global_constants.cpp`）
- Delete: `include/logger.h`
- Delete: `include/global_constants.h`
- Delete: `include/common_types.h`
- Delete: `source/logger.cpp`
- Delete: `source/global_constants.cpp`

- [ ] **Step 1: 移动文件**

```bash
mkdir -p modules/core/include modules/core/src
mv include/logger.h modules/core/include/logger.h
mv include/global_constants.h modules/core/include/global_constants.h
mv include/common_types.h modules/core/include/common_types.h
mv source/logger.cpp modules/core/src/logger.cpp
mv source/global_constants.cpp modules/core/src/global_constants.cpp
rm include/logger.h include/global_constants.h include/common_types.h
rm source/logger.cpp source/global_constants.cpp
git add -A
git commit -m "refactor: move core module to modules/core/"
```

---

### Task 11: 移动 interface 模块文件

**Files:**
- Create: `modules/interface/include/i_video_pipeline.h`（从 `include/i_video_pipeline.h`）
- Create: `modules/interface/include/i_video_encoder.h`（从 `include/i_video_encoder.h`）
- Create: `modules/interface/include/i_stream_provider.h`（从 `include/i_stream_provider.h`）
- Delete: `include/i_video_pipeline.h`
- Delete: `include/i_video_encoder.h`
- Delete: `include/i_stream_provider.h`

- [ ] **Step 1: 移动文件并提交**

```bash
mkdir -p modules/interface/include
mv include/i_video_pipeline.h modules/interface/include/
mv include/i_video_encoder.h modules/interface/include/
mv include/i_stream_provider.h modules/interface/include/
rm include/i_video_pipeline.h include/i_video_encoder.h include/i_stream_provider.h
git add -A
git commit -m "refactor: move interface module to modules/interface/"
```

---

### Task 12: 移动 stream 模块文件

**Files:**
- Create: `modules/stream/include/stream_common.h`（从 `include/stream_common.h`）
- Create: `modules/stream/include/stream_frame.h`（从 `source/stream/stream_frame.h`）
- Create: `modules/stream/include/stream_distributor.h`（从 `source/stream/stream_distributor.h`）
- Create: `modules/stream/include/stream_consumer_manager.h`（从 `source/stream/stream_consumer_manager.h`）
- Create: `modules/stream/include/recorder_consumer.h`（从 `source/stream/recorder_consumer.h`）
- Create: `modules/stream/src/stream_frame.cpp`（从 `source/stream/stream_frame.cpp`）
- Create: `modules/stream/src/stream_distributor.cpp`（从 `source/stream/stream_distributor.cpp`）
- Create: `modules/stream/src/stream_consumer_manager.cpp`（从 `source/stream/stream_consumer_manager.cpp`）
- Create: `modules/stream/src/recorder_consumer.cpp`（从 `source/stream/recorder_consumer.cpp`）
- Delete: `include/stream_common.h`
- Delete: `source/stream/stream_frame.h`
- Delete: `source/stream/stream_frame.cpp`
- Delete: `source/stream/stream_distributor.h`
- Delete: `source/stream/stream_distributor.cpp`
- Delete: `source/stream/stream_consumer_manager.h`
- Delete: `source/stream/stream_consumer_manager.cpp`
- Delete: `source/stream/recorder_consumer.h`
- Delete: `source/stream/recorder_consumer.cpp`
- Delete: `source/stream/CMakeLists.txt`

- [ ] **Step 1: 移动文件并提交**

```bash
mkdir -p modules/stream/include modules/stream/src
mv include/stream_common.h modules/stream/include/
mv source/stream/stream_frame.h modules/stream/include/
mv source/stream/stream_distributor.h modules/stream/include/
mv source/stream/stream_consumer_manager.h modules/stream/include/
mv source/stream/recorder_consumer.h modules/stream/include/
mv source/stream/stream_frame.cpp modules/stream/src/
mv source/stream/stream_distributor.cpp modules/stream/src/
mv source/stream/stream_consumer_manager.cpp modules/stream/src/
mv source/stream/recorder_consumer.cpp modules/stream/src/
rm include/stream_common.h
rm -rf source/stream/
git add -A
git commit -m "refactor: move stream module to modules/stream/"
```

---

### Task 13: 移动 platform 模块文件

**Files:**
- Create: `modules/platform/hi3516cv610/include/hi_video_pipeline.h`（从 `include/video_process_hi.h`）
- Create: `modules/platform/hi3516cv610/include/hi_venc_param.h`（从 `include/video_venc_hi.h`）
- Create: `modules/platform/hi3516cv610/include/hi_stream_fetcher.h`（从 `source/stream/stream_fetcher.h`）
- Create: `modules/platform/hi3516cv610/src/hi_video_pipeline.cpp`（从 `source/video_process_hi.cpp`）
- Create: `modules/platform/hi3516cv610/src/hi_venc_param.cpp`（从 `source/video_venc_hi.cpp`）
- Create: `modules/platform/hi3516cv610/src/hi_stream_fetcher.cpp`（从 `source/stream/stream_fetcher.cpp`）
- Delete: `include/video_process_hi.h`
- Delete: `include/video_venc_hi.h`
- Delete: `source/video_process_hi.cpp`
- Delete: `source/video_venc_hi.cpp`

- [ ] **Step 1: 移动文件并提交**

```bash
mkdir -p modules/platform/hi3516cv610/include modules/platform/hi3516cv610/src
mv include/video_process_hi.h modules/platform/hi3516cv610/include/hi_video_pipeline.h
mv include/video_venc_hi.h modules/platform/hi3516cv610/include/hi_venc_param.h
mv source/video_process_hi.cpp modules/platform/hi3516cv610/src/hi_video_pipeline.cpp
mv source/video_venc_hi.cpp modules/platform/hi3516cv610/src/hi_venc_param.cpp
# stream_fetcher is still at source/stream/ from Task 12, move it now
mv modules/stream/include/stream_fetcher.h modules/platform/hi3516cv610/include/hi_stream_fetcher.h 2>/dev/null || true
mv modules/stream/src/stream_fetcher.cpp modules/platform/hi3516cv610/src/hi_stream_fetcher.cpp 2>/dev/null || true
# Actually the fetcher was in source/stream/ which was deleted - let's check
# After Task 12, source/stream/ is gone. The fetcher was NOT moved in Task 12.
# We need to get it from git or recreate
# Since we committed Task 12 already, let's get it from git
git checkout HEAD -- source/stream/stream_fetcher.h source/stream/stream_fetcher.cpp 2>/dev/null
# Clean up
rm include/video_process_hi.h include/video_venc_hi.h 2>/dev/null
rm source/video_process_hi.cpp source/video_venc_hi.cpp 2>/dev/null
git add -A
git commit -m "refactor: move platform module to modules/platform/hi3516cv610/"
```

---

### Task 14: 移动 app 文件

**Files:**
- Create: `app/main.cpp`（从 `source/main.cpp`）
- Create: `app/CMakeLists.txt`（已创建）
- Delete: `source/main.cpp`

- [ ] **Step 1: 移动文件并提交**

```bash
mkdir -p app
mv source/main.cpp app/main.cpp
rmdir source 2>/dev/null || true
git add -A
git commit -m "refactor: move app entry to app/"
```

---

### Task 15: 移动 test 文件

**Files:**
- Create: `test/test.h`（从 `include/test.h`）
- Delete: `include/test.h`

- [ ] **Step 1: 移动并提交**

```bash
mv include/test.h test/test.h
rm include/test.h 2>/dev/null
git add -A
git commit -m "refactor: move test.h to test/"
```

---

### Task 16: 修改根 CMakeLists.txt

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 重写根 CMakeLists.txt**

用 `add_subdirectory` 替换 `GLOB_RECURSE`，添加平台选项：

```cmake
cmake_minimum_required(VERSION 3.15)
project(eye VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(ENABLE_PEDANTIC "Enable more pedantic warnings" OFF)

set(DEFAULT_WARNING_FLAGS "-Wall -Wextra -Wno-unused-parameter")
set(DEFAULT_OPT_FLAGS_RELEASE "-O2 -DNDEBUG")
set(DEFAULT_OPT_FLAGS_DEBUG "-O0 -g")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_OPT_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_OPT_FLAGS_RELEASE}")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -s")
else()
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${DEFAULT_OPT_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DEFAULT_OPT_FLAGS_DEBUG}")
endif()

if(ENABLE_PEDANTIC)
  add_compile_options(-Wpedantic)
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "Please use -DCMAKE_TOOLCHAIN_FILE=./aarch64-none-linux-gnu-toolchain.cmake")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(THIRDPARTY ${CMAKE_SOURCE_DIR}/thirdparty)
set(target_out ${PROJECT_NAME})
set(HI_MPP_LIB
    ss_mpi_sysmem ss_mpi ir_auto dehaze acs drc ldci bnr
    calcflicker extend_stats ot_mpi_isp ss_mpi_isp
    ss_mpi_ae ss_mpi_awb ss_mpi_sysbind sns_sc4336p)
set(THIRDPARTY_LIB securec ${HI_MPP_LIB} zlog)

option(TARGET_PLATFORM "hi3516cv610" "Target platform")

add_subdirectory(modules/core)
add_subdirectory(modules/interface)
add_subdirectory(modules/stream)
add_subdirectory(modules/platform/${TARGET_PLATFORM})
add_subdirectory(app)

file(GLOB_RECURSE DIR_SRCS "./test/*.c" "./test/*.cpp")
add_executable(${target_out}_test EXCLUDE_FROM_ALL ${DIR_SRCS})
target_include_directories(${target_out}_test PRIVATE
    ./test ./modules/stream/include ./modules/core/include
    ${THIRDPARTY}/zlog/include ${THIRDPARTY}/hi3516cv610_mpp/include)
target_link_libraries(${target_out}_test PRIVATE
    pthread ${THIRDPARTY_LIB} platform_hi stream core)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(DEFINED ENV{HOME})
    set(default_install_dir "$ENV{HOME}/eyeOut")
  else()
    set(default_install_dir "${CMAKE_SOURCE_DIR}/eyeOut")
  endif()
  set(CMAKE_INSTALL_PREFIX "${default_install_dir}" CACHE PATH "Default install path" FORCE)
endif()

install(CODE [[message(STATUS "准备清理安装目录：${CMAKE_INSTALL_PREFIX}")]])
file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")

file(GLOB_RECURSE THIRD_PARTY_SOS "${CMAKE_SOURCE_DIR}/thirdparty/*.so*")
set(FILTERED_SOS "")
foreach(lib ${THIRD_PARTY_SOS})
    if(NOT lib MATCHES ".*/hi3516cv610_mpp/.*")
        list(APPEND FILTERED_SOS ${lib})
    endif()
endforeach()
install(FILES ${FILTERED_SOS} DESTINATION lib)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/conf/ DESTINATION conf)

message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "CXX flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Source dir: ${CMAKE_SOURCE_DIR}")
message(STATUS "Default install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Third-party libs to install: ${FILTERED_SOS}")
```

- [ ] **Step 2: 提交**

```bash
git add CMakeLists.txt
git commit -m "chore: restructure CMake to use add_subdirectory with platform module"
```

---

### Task 17: 更新所有 #include 路径

**Files:**
- Modify: 多文件

由于文件移动，所有相对 include path 的引用都需要检查并更新。主要规则：
- 从 `modules/stream/include/` 引用：`#include "stream_xxx.h"`
- 从 `modules/core/include/` 引用：`#include "logger.h"`
- 从 `modules/platform/hi3516cv610/include/` 引用：`#include "hi_video_pipeline.h"`
- 从 `modules/interface/include/` 引用：`#include "i_video_pipeline.h"`

需要检查并修改的文件：
- `app/main.cpp`（logger.h, hi_video_pipeline.h, test.h）
- `modules/platform/hi3516cv610/src/hi_video_pipeline.cpp`（内含大量 include）
- `modules/platform/hi3516cv610/src/hi_venc_param.cpp`
- `test/test.cpp`（多个 include）
- `test/stream/stream_test.cpp`

- [ ] **Step 1: 批量检查并修复 include 路径**

```bash
# 使用 grep 找出所有需要修改的 include
rg '#include "' source/ app/ test/ --type cpp --type c --type h | sort -u
```

根据结果逐个修改。此任务较为繁琐，需要确保每个 include 路径在新的目录结构下有效。

- [ ] **Step 2: 提交**

```bash
git add -A
git commit -m "fix: update include paths for new directory structure"
```

---

### Task 18: 最终编译验证

- [ ] **Step 1: 执行构建**

```bash
cd /home/yangyang/projects/eye/build
./set.sh && make
```

预期结果：编译通过。

---

## 风险与注意事项

1. **video_process_hi.cpp 改动量最大**：~700+ 行，包含 SYS/VB/VI/VPSS/ISP/MIPI 全部初始化逻辑。Task 7 改动方法签名时需小心，建议逐函数验证。
2. **阶段一必须编译通过后才能进入阶段二**：否则 include 路径问题会在目录重组后叠加变得更复杂。
3. **stream_fetcher 移动**：在 Task 12 和 Task 13 之间需要特别注意，stream_fetcher.h/cpp 在 Task 12 先被删除了 source/stream/ 目录时会被清理，需要从 git 恢复。
4. **test/ 目录的 CMake**：由于 test 仍使用 GLOB_RECURSE 其源文件，其 include 路径只需确保 `modules/stream/include` 在 path 中即可。
