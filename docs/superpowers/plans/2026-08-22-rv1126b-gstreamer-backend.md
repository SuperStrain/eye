# RV1126B GStreamer 后端兼容 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 RV1126B 平台新增一个 GStreamer 媒体后端，与现有 rockit 后端通过编译期宏 `USE_GSTREAMER` 二选一，共用同一套 `IVideoPipeline`/`IStreamProvider` 工厂契约与上层 stream/RTSP 链路。

**Architecture:** 双后端类 + 宏分发工厂。保留 rockit 的 `rv_*` 文件零改动；新增 `gst_video_pipeline`（GStreamer 管线：`v4l2src → tee → 3 分支 → mpp 编码器 → parse → appsink`）与 `gst_stream_fetcher`（appsink 拉流 → `FrameData` → `StreamDistributor`）；`rv_platform_factory.cpp` 用 `#ifdef USE_GSTREAMER` 分发；CMake 选项 + `Makefile` 变量透传，按后端检测依赖库。

**Tech Stack:** C++11、CMake ≥ 3.15、GStreamer 1.24.13（`gstreamer-1.0`/`gstreamer-app-1.0`，交叉编译 SDK）、Rockchip MPP 插件（`mpph264enc`/`mpph265enc`/`mppjpegenc`）、现有 librockit（rockit 后端）。

**Spec:** `docs/superpowers/specs/2026-08-22-rv1126b-gstreamer-backend-design.md`

## Global Constraints

- 后端选择：CMake `option(USE_GSTREAMER ... OFF)`，默认 OFF=rockit；ON 时定义编译宏 `USE_GSTREAMER=1` 并链接 GStreamer。
- 码流参数（对齐 rockit，主码流分辨率按 v4l2 实测）：主 H265 **3840×2160@30 8192kbps**、子 H264 **1280×720@30 2048kbps**、MJPEG **1280×720@10**；源 `v4l2src device=/dev/video-camera0 io-mode=mmap`，格式 **NV16**。
- 不修改 `modules/interface`、`modules/stream`、`modules/rtsp`、`app/main.cpp` 的任何文件与对外契约。
- rockit 后端文件 `rv_video_pipeline.{h,cpp}`、`rv_stream_fetcher.{h,cpp}` 零改动。
- 日志分类 `GST`：仅 `modules/core/include/logger.h` 的 `LOG_CATEGORY_LIST` 加 `X(GST)`；zlog 通配规则自动覆盖，不改 `conf/zlog.conf`。
- 构建前 PATH 需含对应 SDK 的 `aarch64-buildroot-linux-gnu-gcc`；依赖库缺失时 configure 直接报错（检测而非硬编码路径）。
- 交叉编译产物为 ARM 二进制，**禁止本地运行**；每任务的验证以“交叉编译/链接成功 + 代码审查”为主，运行期验证集中在 Task 4（设备侧）。

## File Structure

| 文件 | 动作 | 职责 |
|---|---|---|
| `modules/platform/rv1126b/include/gst_video_pipeline.h` | 新增 | `GstVideoPipeline` 声明（`IVideoPipeline`+`IVideoEncoder`，单例，`app_sink()`） |
| `modules/platform/rv1126b/src/gst_video_pipeline.cpp` | 新增 | 管线描述构建、`init/deinit/start/stop`、appsink 获取 |
| `modules/platform/rv1126b/include/gst_stream_fetcher.h` | 新增 | `GstStreamFetcher` 声明（`IStreamProvider`） |
| `modules/platform/rv1126b/src/gst_stream_fetcher.cpp` | 新增 | appsink 拉流 → `FrameData` → `StreamFrame` → `StreamDistributor` |
| `modules/platform/rv1126b/src/rv_platform_factory.cpp` | 修改 | `#ifdef USE_GSTREAMER` 分发到 `Gst*` / `Rv*` |
| `modules/platform/rv1126b/CMakeLists.txt` | 修改 | `option` + 依赖检测 + 按宏选源/库 |
| `Makefile` | 修改 | 透传 `USE_GSTREAMER`，help 说明 |
| `modules/core/include/logger.h` | 修改 | 加 `X(GST)` |

命名空间：`GstVideoPipeline`、`GstStreamFetcher` 均在 `rv1126bMedia` 命名空间（与 `RvVideoPipeline` 一致）；`RvStreamFetcher` 为全局命名空间（现状），工厂里分别按命名空间引用。

---

### Task 1: 构建开关 + 依赖检测 + 日志 + 工厂分发 + 编译骨架

**Files:**
- Modify: `Makefile`
- Modify: `modules/platform/rv1126b/CMakeLists.txt`
- Modify: `modules/core/include/logger.h`
- Modify: `modules/platform/rv1126b/src/rv_platform_factory.cpp`
- Create: `modules/platform/rv1126b/include/gst_video_pipeline.h`
- Create: `modules/platform/rv1126b/include/gst_stream_fetcher.h`
- Create: `modules/platform/rv1126b/src/gst_video_pipeline.cpp`（本任务为最小骨架，Task 2 填完整实现）
- Create: `modules/platform/rv1126b/src/gst_stream_fetcher.cpp`（本任务为最小骨架，Task 3 填完整实现）

**Interfaces:**
- Consumes: 现有 `IVideoPipeline` / `IVideoEncoder` / `IStreamProvider` / `platform_factory.h` / `StreamDistributor`。
- Produces: `rv1126bMedia::GstVideoPipeline::getInstance()`、`rv1126bMedia::GstVideoPipeline::app_sink(VencChannel)->GstAppSink*`、`rv1126bMedia::GstStreamFetcher(chn,type,codec,distributor)`——Task 2/3 依赖这些签名。

- [ ] **Step 1: 加日志分类 `GST`**

`modules/core/include/logger.h` 的 `LOG_CATEGORY_LIST` 在 `X(ROCKIT)` 后加 `X(GST)`：

```c
#define LOG_CATEGORY_LIST \
    X(HIMPP)              \
    X(ROCKIT)             \
    X(GST)                \
    X(TEST)               \
    X(STREAM)             \
    X(RTSP)
```

- [ ] **Step 2: Makefile 透传 `USE_GSTREAMER`**

`Makefile` 顶部（`TARGET_PLATFORM ?= hi3516cv610` 之后）加默认值；`CMAKE_CFG` 加 `-DUSE_GSTREAMER`；`help` 加一行。

```make
TARGET_PLATFORM ?= hi3516cv610
USE_GSTREAMER ?= OFF
```

```make
CMAKE_CFG := cmake -S . -B $(BUILD_DIR) \
    -DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) \
    -DTARGET_PLATFORM=$(TARGET_PLATFORM) \
    -DUSE_GSTREAMER=$(USE_GSTREAMER) \
    -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
```

`help` 的变量段补：

```make
	@echo "  USE_GSTREAMER=ON                        rv1126b 用 GStreamer 后端（默认 OFF=rockit）"
```

- [ ] **Step 3: CMakeLists.txt 加选项、依赖检测、按宏选源/库**

整体替换 `modules/platform/rv1126b/CMakeLists.txt`：

```cmake
option(USE_GSTREAMER "Use GStreamer media backend (default: rockit)" OFF)

if(USE_GSTREAMER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)

    add_library(platform_impl STATIC
        src/gst_video_pipeline.cpp
        src/gst_stream_fetcher.cpp
        src/rv_platform_factory.cpp
    )
    target_include_directories(platform_impl PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/modules/stream/include
    )
    target_link_libraries(platform_impl
        PUBLIC interface
        PRIVATE PkgConfig::GST pthread
    )
    target_compile_definitions(platform_impl PUBLIC PLATFORM_RV1126B=1 USE_GSTREAMER=1)
else()
    find_path(ROCKIT_INCLUDE_DIR rk_mpi_sys.h REQUIRED)
    find_library(ROCKIT_LIBRARY rockit REQUIRED)

    add_library(platform_impl STATIC
        src/rv_video_pipeline.cpp
        src/rv_stream_fetcher.cpp
        src/rv_platform_factory.cpp
    )
    target_include_directories(platform_impl PUBLIC
        include
        ${CMAKE_SOURCE_DIR}/modules/stream/include
        ${ROCKIT_INCLUDE_DIR}
    )
    target_link_libraries(platform_impl
        PUBLIC interface
        PRIVATE ${ROCKIT_LIBRARY} pthread
    )
    target_compile_definitions(platform_impl PUBLIC PLATFORM_RV1126B=1)
endif()
```

- [ ] **Step 4: 工厂分发**

整体替换 `modules/platform/rv1126b/src/rv_platform_factory.cpp`：

```cpp
#include "platform_factory.h"

#ifdef USE_GSTREAMER
#include "gst_stream_fetcher.h"
#include "gst_video_pipeline.h"
#else
#include "rv_stream_fetcher.h"
#include "rv_video_pipeline.h"
#endif

IVideoPipeline& platform_video_pipeline() {
#ifdef USE_GSTREAMER
    return rv1126bMedia::GstVideoPipeline::getInstance();
#else
    return rv1126bMedia::RvVideoPipeline::getInstance();
#endif
}

std::unique_ptr<IStreamProvider> create_stream_fetcher(
    VencChannel chn,
    StreamType type,
    CodecType codec,
    StreamDistributor& distributor) {
#ifdef USE_GSTREAMER
    return std::unique_ptr<IStreamProvider>(
        new rv1126bMedia::GstStreamFetcher(chn, type, codec, distributor));
#else
    return std::unique_ptr<IStreamProvider>(
        new RvStreamFetcher(chn, type, codec, distributor));
#endif
}
```

- [ ] **Step 5: `gst_video_pipeline.h`（最终版头文件，后续不再改）**

`modules/platform/rv1126b/include/gst_video_pipeline.h`：

```cpp
#ifndef GST_VIDEO_PIPELINE_H
#define GST_VIDEO_PIPELINE_H

#include "i_video_encoder.h"
#include "i_video_pipeline.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

namespace rv1126bMedia {

class GstVideoPipeline : public IVideoPipeline, public IVideoEncoder {
public:
    GstVideoPipeline(const GstVideoPipeline&) = delete;
    GstVideoPipeline& operator=(const GstVideoPipeline&) = delete;

    static GstVideoPipeline& getInstance();

    int init() override;
    int deinit() override;
    int start() override;
    int stop() override;

    int createChannel(int chn, CodecType codec, Size resolution) override;
    int destroyChannel(int chn) override;
    int startChannel(int chn) override;
    int stopChannel(int chn) override;

    GstAppSink* app_sink(VencChannel chn);

private:
    GstVideoPipeline();
    ~GstVideoPipeline();

    GstElement* pipeline_;
    GstAppSink* app_sinks_[3];
    bool initialized_;
};

} // namespace rv1126bMedia

#endif
```

- [ ] **Step 6: `gst_stream_fetcher.h`（最终版头文件，后续不再改）**

`modules/platform/rv1126b/include/gst_stream_fetcher.h`：

```cpp
#ifndef GST_STREAM_FETCHER_H
#define GST_STREAM_FETCHER_H

#include "i_stream_provider.h"
#include "stream_distributor.h"
#include <atomic>
#include <thread>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

namespace rv1126bMedia {

class GstStreamFetcher : public IStreamProvider {
public:
    GstStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                     StreamDistributor& distributor);
    ~GstStreamFetcher();

    int start() override;
    int stop() override;
    int fetchFrame(VencChannel chn, FrameData& frame) override;
    int releaseFrame(VencChannel chn) override;

private:
    void run();

    VencChannel channel_;
    StreamType stream_type_;
    CodecType codec_type_;
    StreamDistributor& distributor_;
    GstAppSink* app_sink_;
    uint32_t seq_;
    std::atomic<bool> running_;
    std::thread thread_;
};

} // namespace rv1126bMedia

#endif
```

- [ ] **Step 7: `gst_video_pipeline.cpp` 最小骨架（本任务先能编译，Task 2 填实现）**

`modules/platform/rv1126b/src/gst_video_pipeline.cpp`：

```cpp
#include "gst_video_pipeline.h"

#include <cstring>

namespace rv1126bMedia {

GstVideoPipeline& GstVideoPipeline::getInstance() {
    static GstVideoPipeline instance;
    return instance;
}

GstVideoPipeline::GstVideoPipeline() : pipeline_(nullptr), initialized_(false) {
    std::memset(app_sinks_, 0, sizeof(app_sinks_));
}

GstVideoPipeline::~GstVideoPipeline() {}

int GstVideoPipeline::init() { return 0; }
int GstVideoPipeline::deinit() { return 0; }
int GstVideoPipeline::start() { return 0; }
int GstVideoPipeline::stop() { return 0; }
int GstVideoPipeline::createChannel(int, CodecType, Size) { return 0; }
int GstVideoPipeline::destroyChannel(int) { return 0; }
int GstVideoPipeline::startChannel(int) { return 0; }
int GstVideoPipeline::stopChannel(int) { return 0; }

GstAppSink* GstVideoPipeline::app_sink(VencChannel) { return nullptr; }

} // namespace rv1126bMedia
```

- [ ] **Step 8: `gst_stream_fetcher.cpp` 最小骨架（本任务先能编译，Task 3 填实现）**

`modules/platform/rv1126b/src/gst_stream_fetcher.cpp`：

```cpp
#include "gst_stream_fetcher.h"

namespace rv1126bMedia {

GstStreamFetcher::GstStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                                   StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec), distributor_(distributor),
      app_sink_(nullptr), seq_(0), running_(false) {}

GstStreamFetcher::~GstStreamFetcher() { stop(); }

int GstStreamFetcher::start() {
    if (running_) return 0;
    running_ = true;
    thread_ = std::thread(&GstStreamFetcher::run, this);
    return 0;
}

int GstStreamFetcher::stop() {
    if (!running_) return 0;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    return 0;
}

int GstStreamFetcher::fetchFrame(VencChannel, FrameData&) { return -1; }
int GstStreamFetcher::releaseFrame(VencChannel) { return 0; }

void GstStreamFetcher::run() {
    while (running_) {
        // 骨架占位；Task 3 实现拉流。
    }
}

} // namespace rv1126bMedia
```

- [ ] **Step 9: 交叉编译 GStreamer 后端，确认 configure+编译+链接通过**

```bash
export PATH=/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/bin:$PATH
cd /home/yangyang/projects/eye
USE_GSTREAMER=ON TARGET_PLATFORM=rv1126b make
```

Expected: 配置阶段 `pkg_check_modules` 找到 gstreamer-1.0 / gstreamer-app-1.0，编译链接成功，生成 `build/rv1126b-release/app/eye`。

若 configure 报 `No package 'gstreamer-1.0' found`，在 `CMakeLists.txt` 的 `pkg_check_modules` 之前补两行再重试：

```cmake
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
    set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/pkgconfig")
```

- [ ] **Step 10: 交叉编译 rockit 后端（默认 OFF），确认无回归**

```bash
export PATH=/home/yangyang/projects/rv1126b_linux_build/buildroot/output/alientek_rv1126b_ipc/host/bin:$PATH
cd /home/yangyang/projects/eye
make clean && TARGET_PLATFORM=rv1126b make
```

Expected: `find_path`/`find_library` 找到 `rk_mpi_sys.h` + `librockit`，编译链接成功（rockit 文件零改动，行为不变）。

> 注意：`make clean` 会清 `build/rv1126b-release/`，避免上一次 GStreamer 配置残留；切换后端时都先 `make clean`。

- [ ] **Step 11: 校验产物依赖，确认后端切换生效**

```bash
# GStreamer 后端产物应依赖 gstreamer，不依赖 rockit
readelf -d build/rv1126b-release/app/eye | grep -iE 'NEEDED.*(gst|rockit)'
```

Expected（GStreamer 构建后）：含 `libgstreamer-1.0.so`、`libgstapp-1.0.so`，无 `librockit.so`。

- [ ] **Step 12: Commit**

```bash
cd /home/yangyang/projects/eye
git add Makefile modules/platform/rv1126b modules/core/include/logger.h
git commit -m "feat(rv1126b): 新增 GStreamer 后端构建开关与工厂分发骨架"
```

---

### Task 2: GstVideoPipeline 完整实现

**Files:**
- Modify: `modules/platform/rv1126b/src/gst_video_pipeline.cpp`（整体替换骨架）

**Interfaces:**
- Consumes: `GstVideoPipeline` 头文件（Task 1）、`logger.h` 的 `GST` 分类。
- Produces: `init()` 构建并 PAUSED 管线、`start()` 置 PLAYING、`app_sink(VencChannel)->GstAppSink*`——Task 3 的 fetcher 依赖 `app_sink()` 与 `start()`。

- [ ] **Step 1: 完整实现 `gst_video_pipeline.cpp`**

整体替换 `modules/platform/rv1126b/src/gst_video_pipeline.cpp`：

```cpp
#include "gst_video_pipeline.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "logger.h"

namespace rv1126bMedia {

namespace {

// 与用户实测一致的 v4l2 源：/dev/video-camera0，NV16 3840x2160@30，io-mode=mmap。
constexpr char kDevice[] = "/dev/video-camera0";
constexpr char kRawFormat[] = "NV16";
constexpr int kMainWidth = 3840;
constexpr int kMainHeight = 2160;
constexpr int kSubWidth = 1280;
constexpr int kSubHeight = 720;
constexpr int kFrameRate = 30;
constexpr int kGop = 60;                  // kFrameRate * 2
constexpr int kMainBitrateBps = 8192000;  // 8192 kbps
constexpr int kSubBitrateBps = 2048000;   // 2048 kbps
constexpr int kMjpegFrameRate = 10;

constexpr char kAppSinkNames[][16] = {"app_main", "app_sub", "app_mjpeg"};

std::string build_pipeline_desc() {
    char buf[1024];
    std::snprintf(buf, sizeof(buf),
        "v4l2src device=%s io-mode=mmap ! "
        "video/x-raw,format=%s,width=%d,height=%d,framerate=%d/1 ! tee name=t "
        "t. ! queue ! mpph265enc rc-mode=cbr bps=%d gop=%d ! "
        "h265parse ! video/x-h265,stream-format=byte-stream,alignment=au ! appsink name=app_main "
        "t. ! queue ! videoscale ! video/x-raw,width=%d,height=%d ! "
        "mpph264enc rc-mode=cbr bps=%d gop=%d ! "
        "h264parse ! video/x-h264,stream-format=byte-stream,alignment=au ! appsink name=app_sub "
        "t. ! queue ! videoscale ! video/x-raw,width=%d,height=%d ! "
        "videorate ! video/x-raw,framerate=%d/1 ! "
        "mppjpegenc ! appsink name=app_mjpeg",
        kDevice, kRawFormat, kMainWidth, kMainHeight, kFrameRate,
        kMainBitrateBps, kGop,
        kSubWidth, kSubHeight, kSubBitrateBps, kGop,
        kSubWidth, kSubHeight, kMjpegFrameRate);
    return std::string(buf);
}

} // namespace

GstVideoPipeline& GstVideoPipeline::getInstance() {
    static GstVideoPipeline instance;
    return instance;
}

GstVideoPipeline::GstVideoPipeline() : pipeline_(nullptr), initialized_(false) {
    std::memset(app_sinks_, 0, sizeof(app_sinks_));
}

GstVideoPipeline::~GstVideoPipeline() {}

int GstVideoPipeline::init() {
    if (initialized_) return 0;

    GError* error = nullptr;
    if (!gst_init_check(nullptr, nullptr, &error)) {
        LOGGER_ERROR(GST, "gst_init_check failed: %s", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return -1;
    }

    std::string desc = build_pipeline_desc();
    LOGGER_INFO(GST, "pipeline: %s", desc.c_str());

    pipeline_ = gst_parse_launch(desc.c_str(), &error);
    if (!pipeline_) {
        LOGGER_ERROR(GST, "gst_parse_launch failed: %s", error ? error->message : "unknown");
        if (error) g_error_free(error);
        return -1;
    }

    for (int i = 0; i < 3; ++i) {
        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), kAppSinkNames[i]);
        if (!sink) {
            LOGGER_ERROR(GST, "appsink %s not found", kAppSinkNames[i]);
            deinit();
            return -1;
        }
        app_sinks_[i] = GST_APP_SINK(sink);
        gst_object_unref(sink);  // 归还 get_by_name 的引用，pipeline 仍持有元素
    }

    // 到 PAUSED 验证所有元素可加载/可协商；真正 PLAYING 由 start() 触发（首个 fetcher 调用）。
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGGER_ERROR(GST, "pipeline failed to reach PAUSED");
        deinit();
        return -1;
    }

    initialized_ = true;
    LOGGER_INFO(GST, "RV1126B GStreamer pipeline initialized");
    return 0;
}

int GstVideoPipeline::deinit() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    std::memset(app_sinks_, 0, sizeof(app_sinks_));
    initialized_ = false;
    return 0;
}

int GstVideoPipeline::start() {
    if (!initialized_ || !pipeline_) return -1;
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOGGER_ERROR(GST, "pipeline failed to reach PLAYING");
        return -1;
    }
    return 0;
}

int GstVideoPipeline::stop() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }
    return 0;
}

int GstVideoPipeline::createChannel(int, CodecType, Size) { return 0; }
int GstVideoPipeline::destroyChannel(int) { return 0; }
int GstVideoPipeline::startChannel(int) { return 0; }
int GstVideoPipeline::stopChannel(int) { return 0; }

GstAppSink* GstVideoPipeline::app_sink(VencChannel chn) {
    int idx = static_cast<int>(chn);
    if (idx < 0 || idx >= 3) return nullptr;
    return app_sinks_[idx];
}

} // namespace rv1126bMedia
```

- [ ] **Step 2: 交叉编译 GStreamer 后端**

```bash
export PATH=/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/bin:$PATH
cd /home/yangyang/projects/eye
USE_GSTREAMER=ON TARGET_PLATFORM=rv1126b make
```

Expected: 编译链接通过，无警告（`-Wall -Wextra`，`-Wno-unused-parameter`）。

- [ ] **Step 3: Commit**

```bash
cd /home/yangyang/projects/eye
git add modules/platform/rv1126b/src/gst_video_pipeline.cpp
git commit -m "feat(rv1126b): GStreamer 管线构建与启停实现"
```

---

### Task 3: GstStreamFetcher 完整实现

**Files:**
- Modify: `modules/platform/rv1126b/src/gst_stream_fetcher.cpp`（整体替换骨架）

**Interfaces:**
- Consumes: `GstVideoPipeline::getInstance().app_sink(VencChannel)`、`GstVideoPipeline::getInstance().start()`（Task 2）、`StreamFrame` / `StreamDistributor::push`。
- Produces: `GstStreamFetcher::start/stop/fetchFrame/releaseFrame` 完整行为——`app/main.cpp` 经 `create_stream_fetcher` 使用。

- [ ] **Step 1: 完整实现 `gst_stream_fetcher.cpp`**

整体替换 `modules/platform/rv1126b/src/gst_stream_fetcher.cpp`：

```cpp
#include "gst_stream_fetcher.h"
#include "gst_video_pipeline.h"
#include "logger.h"
#include "stream_frame.h"

#include <cstdio>
#include <pthread.h>

namespace rv1126bMedia {

namespace {

// 关键帧判定：无 DELTA_UNIT 标志即 IDR（h264parse/h265parse 已按 AU 输出）。
bool is_keyframe(GstBuffer* buffer) {
    return !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
}

} // namespace

GstStreamFetcher::GstStreamFetcher(VencChannel chn, StreamType type, CodecType codec,
                                   StreamDistributor& distributor)
    : channel_(chn), stream_type_(type), codec_type_(codec), distributor_(distributor),
      app_sink_(GstVideoPipeline::getInstance().app_sink(chn)),
      seq_(0), running_(false) {}

GstStreamFetcher::~GstStreamFetcher() { stop(); }

int GstStreamFetcher::start() {
    if (running_) return 0;
    // 应用侧未显式调 IVideoPipeline::start()，由首个 fetcher 触发管线 PLAYING。
    int ret = GstVideoPipeline::getInstance().start();
    if (ret != 0) return ret;
    running_ = true;
    thread_ = std::thread(&GstStreamFetcher::run, this);
    return 0;
}

int GstStreamFetcher::stop() {
    if (!running_) return 0;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    return 0;
}

int GstStreamFetcher::fetchFrame(VencChannel chn, FrameData& frame) {
    (void)chn;
    if (!app_sink_) return -1;

    GstSample* sample = gst_app_sink_try_pull_sample(app_sink_, 100 * GST_MSECOND);
    if (!sample) return -1;  // 100ms 超时，run() 里继续检查 running_

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return -1;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return -1;
    }

    frame.pack_count = 1;
    frame.seq = seq_++;
    frame.packs[0].data = map.data;
    frame.packs[0].len = static_cast<uint32_t>(map.size);
    if (codec_type_ == CodecType::MJPEG) {
        frame.packs[0].nalu_type = NaluType::IDR_SLICE;
    } else {
        frame.packs[0].nalu_type = is_keyframe(buffer) ? NaluType::IDR_SLICE : NaluType::P_SLICE;
    }

    // StreamFrame 构造时深拷贝；随后立即 unmap/unref，不把 GStreamer 缓冲指针带出构造。
    auto stream_frame = std::make_shared<StreamFrame>(channel_, stream_type_, codec_type_, frame);

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    distributor_.push(stream_frame);
    return 0;
}

int GstStreamFetcher::releaseFrame(VencChannel) {
    return 0;
}

void GstStreamFetcher::run() {
    char name[16];
    std::snprintf(name, sizeof(name), "GstFetch_%d", static_cast<int>(channel_));
    pthread_setname_np(pthread_self(), name);

    LOGGER_INFO(STREAM, "GStreamer fetcher ch%d running", static_cast<int>(channel_));
    while (running_) {
        FrameData frame;
        fetchFrame(channel_, frame);  // 失败即 100ms 超时，继续循环
    }
    LOGGER_INFO(STREAM, "GStreamer fetcher ch%d stopped", static_cast<int>(channel_));
}

} // namespace rv1126bMedia
```

- [ ] **Step 2: 交叉编译 GStreamer 后端**

```bash
export PATH=/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/bin:$PATH
cd /home/yangyang/projects/eye
USE_GSTREAMER=ON TARGET_PLATFORM=rv1126b make
```

Expected: 编译链接通过。

- [ ] **Step 3: 回归编译 rockit 后端**

```bash
export PATH=/home/yangyang/projects/rv1126b_linux_build/buildroot/output/alientek_rv1126b_ipc/host/bin:$PATH
cd /home/yangyang/projects/eye
make clean && TARGET_PLATFORM=rv1126b make
```

Expected: rockit 后端编译链接通过（`rv_*` 与工厂 else 分支未受影响）。

- [ ] **Step 4: 安装 GStreamer 后端产物**

```bash
export PATH=/opt/aarch64-buildroot-linux-gnu_sdk-buildroot/bin:$PATH
cd /home/yangyang/projects/eye
USE_GSTREAMER=ON TARGET_PLATFORM=rv1126b make install
```

Expected: `$HOME/eyeOut/rv1126b/bin/eye` 更新；`readelf -d` 确认含 gstreamer 依赖、无 librockit。

- [ ] **Step 5: Commit**

```bash
cd /home/yangyang/projects/eye
git add modules/platform/rv1126b/src/gst_stream_fetcher.cpp
git commit -m "feat(rv1126b): GStreamer appsink 拉流与 StreamFrame 分发实现"
```

---

### Task 4: 设备侧端到端验证

**Files:** 无代码改动（验证任务）。

**Interfaces:**
- Consumes: Task 1-3 产出的 GStreamer 后端 `eye` 二进制。

> 前置：把 `$HOME/eyeOut/rv1126b/bin/eye` 部署到 eye 固件的 `/app/bin/eye`（`S99eyeout` 启动脚本会 `export LD_LIBRARY_PATH=$APP_DIR/lib` 后运行）。设备须为 GStreamer+MPP 固件（含 `libgstrockchipmpp.so`/`libgstvideo4linux2.so`/`libgstapp.so`，无 librockit）。

- [ ] **Step 1: 启动并核对初始化日志**

设备上运行 `/app/bin/eye`，观察 stdout/日志：`GST` 分类输出 `pipeline: v4l2src device=/dev/video-camera0 ...` 与 `RV1126B GStreamer pipeline initialized`；无 `gst_parse_launch failed` / `pipeline failed to reach PAUSED`。

- [ ] **Step 2: RTSP 主/子码流拉流**

```bash
ffprobe rtsp://<设备IP>:8554/main
ffprobe rtsp://<设备IP>:8554/sub
```

Expected: main = H265 **3840×2160@30**、sub = H264 **1280×720@30**，起播无花屏（首帧含 SPS/PPS + IDR）。

- [ ] **Step 3: 核对 `stream_test()` 产物**

设备上运行 `eye` 后约 10~15 秒（`stream_test` 先等 10s ISP 预热再录 5s），检查：

```bash
ls -l /run/stream_chn0.h265 /run/stream_chn1.h264 /run/stream_chn2.mjpeg
ffprobe /run/stream_chn0.h265   # 应识别为 HEVC
ffprobe /run/stream_chn1.h264   # 应识别为 H.264
ffprobe /run/stream_chn2.mjpeg  # 应识别为 MJPEG/JPEG 序列
```

Expected: 三个文件非空，`ffprobe` 能解析。

- [ ] **Step 4: 稳定性观察**

持续运行 5~10 分钟，观察：无 appsink 丢帧/编码器报错、无内存持续增长、`top`/`perf top -p $(pidof eye)` 查看 CPU（两路 `videoscale` 缩放 + 硬编）。

- [ ] **Step 5: 记录结果到 spec 的验证节**

若子/MJPEG 分支需要 `videoconvert` 兜底（源 NV16 缩放兼容问题），按 spec §5.3 在该分支加 `videoconvert ! video/x-raw,format=NV12`，改后回到 Task 3 Step 2 重编译验证，并更新 spec §7 风险表对应行。

---

## 自审记录

- **Spec 覆盖**：宏切换（Task 1）、依赖检测（Task 1）、日志 `GST`（Task 1）、`GstVideoPipeline` 管线/启停（Task 2）、`GstStreamFetcher` 拉流映射（Task 3）、设备验证（Task 4）——覆盖 spec §5.2~§5.6 与 §9。
- **占位符扫描**：无 TBD/TODO；所有代码步骤均给出完整代码。
- **类型/命名一致性**：`app_sink(VencChannel)->GstAppSink*`、`GstVideoPipeline::getInstance()`、`rv1126bMedia` 命名空间在 Task 1~3 中一致；工厂 else 分支 `RvStreamFetcher`（全局）与 `rv1126bMedia::RvVideoPipeline`（命名空间）与现状一致。
- **生命周期**：`init()` 只到 PAUSED，PLAYING 由 `GstStreamFetcher::start()` 经 `GstVideoPipeline::start()` 触发（应用未显式调 `IVideoPipeline::start()`，与 `stream_test` 里 `start_all()` 的时机对齐）。
