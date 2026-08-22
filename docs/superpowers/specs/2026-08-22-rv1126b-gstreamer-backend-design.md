# RV1126B GStreamer 后端兼容设计

- 日期：2026-08-22
- 状态：已定稿，待实现
- 适用平台：RV1126B（aarch64）
- 影响范围：`modules/platform/rv1126b/`、`Makefile`、`modules/core/include/logger.h`（新增日志分类）

## 1. 背景与问题

eye 的 RV1126B 平台实现 `modules/platform/rv1126b/` 目前只走瑞芯微 **librockit**（`rk_mpi_*`
API）：`RvVideoPipeline` 负责 VI + VENC + 绑定，`RvStreamFetcher` 用 `RK_MPI_VENC_GetStream`
拉码流。上层 `modules/stream`（`StreamFrame` / `StreamDistributor`）与 `modules/rtsp`（Live555）
只消费平台无关的 `StreamFrame`，对底层媒体框架无感知。

实际板端固件（正点原子 RV1126B，`alientek_rv1126b_mpp_eye_defconfig`）**未启用 rockit**
（`# BR2_PACKAGE_ROCKIT is not set`），而是启用 GStreamer 1.24.13 + Rockchip MPP 插件
（`gstreamer1-rockchip`，提供 `mpph264enc/mpph265enc/mppjpegenc`）+ `video4linux2`（`v4l2src`）
+ `app`（`appsink`）+ `librockchip_mpp`/`librga`。这导致 rockit 版二进制在 eye 固件上缺少
`librockit.so` 而无法运行。

本任务为 RV1126B 平台新增一个 **GStreamer 媒体后端**，与现有 rockit 后端通过编译期宏二选一，
共用同一套 `IVideoPipeline` / `IStreamProvider` 工厂契约与上层 stream/RTSP 链路。

## 2. 目标与非目标

### 目标

1. 用编译期宏 `USE_GSTREAMER` 在 rockit / GStreamer 两个后端之间二选一，二者**完全对等**。
2. GStreamer 后端复刻 rockit 当前的码流语义：主码流 H.265、子码流 H.264、MJPEG 三路，分辨率/
   帧率/码率与 rockit 一致。
3. 不改上层 `interface` / `stream` / `rtsp` / `app` 的对外契约；GStreamer 后端把编码帧装进现有
   `StreamFrame`，后续 RTSP/recorder 消费链零改动。
4. rockit 后端保持现状可编译可运行，零行为变化。

### 非目标

- 不做运行时双后端并存/动态切换（用户明确要编译期宏）。
- 不迁移 `modules/rtsp` 到 GStreamer 的 rtsp-server（保留现有 Live555 RTSP）。
- 不引入通用配置化 / 多 sensor profile 抽象（YAGNI，参数仍为常量）。
- 不处理音频（`AUDIO_AAC` 枚举现状即未使用）。
- 首版不优化子码流/MJPEG 的缩放路径（用 `videoscale`，后续可换 ISP selfpath）。

## 3. 关键事实依据

### 3.1 现状 rockit 参数（`rv_video_pipeline.cpp`，已适配 IMX415）

| 常量 | 值 | 说明 |
|---|---|---|
| `kMainWidth/Height` | 3864×2192 | IMX415 mode[0] native，ISP 无缩放 |
| `kSubWidth/Height` | 1280×720 | ISP 下采样 |
| `kMjpegWidth/Height` | 1280×720 | 随 VI Chn2 广播 |
| `kFrameRate` | 30 | — |
| `kMainBitrate` | 8192 kbps | H265 CBR |
| `kSubBitrate` | 2048 kbps | H264 CBR |
| `kMjpegBitrate` | 2048 kbps | MJPEG CBR，dst 帧率 10 |

通道映射：`VencChannel::CHN0`=H265 主码流、`CHN1`=H264 子码流、`CHN2`=MJPEG；VI Chn3=主、
Chn2=子（同时广播给 CHN1/CHN2）。

### 3.2 接口契约（不改）

- `platform_factory.h`：`platform_video_pipeline()` → `IVideoPipeline&`；
  `create_stream_fetcher(chn,type,codec,distributor)` → `unique_ptr<IStreamProvider>`。
- `IVideoPipeline`：`init/deinit/start/stop`；`IVideoEncoder`：`createChannel/destroyChannel/
  startChannel/stopChannel`（rockit 现状即空实现，GStreamer 保持一致）。
- `IStreamProvider`：`start/stop/fetchFrame/releaseFrame`。
- `FrameData`：最多 16 个 `FramePack{data,len,nalu_type}`；`StreamFrame` 构造时深拷贝 pack 数据。
- RTSP 层 `split_annex_b_nals` 期望 H.264/H.265 为 **Annex-B**（start code 00 00 01 / 00 00 00 01）。

### 3.3 GStreamer 可用性（已核实）

- eye 固件 target rootfs 含：`libgstrockchipmpp.so`（`mpph264enc/mpph265enc/mppjpegenc`）、
  `libgstvideo4linux2.so`（`v4l2src`）、`libgstapp.so`（`appsink`）、`libgstx264.so`、
  `libgstrtp/rtsp/udp/tcp.so`、`librockchip_mpp.so`、`librga.so`；**无 librockit**。
- 交叉编译 sysroot（`/opt/aarch64-buildroot-linux-gnu_sdk-buildroot`）：有 `gstreamer-1.0` /
  `gstreamer-app-1.0` / `glib-2.0` / `gobject-2.0` 的 `.pc` 与头文件、`libgstapp-1.0.so`；
  **无 rockit**（无 `rk_mpi_sys.h`、无 `librockit.so`）。
- rockit 头/库在 `buildroot/output/alientek_rv1126b_ipc/host/.../sysroot`（该 sysroot 无 gstreamer）。
- `gstreamer1-rockchip` 编码器属性（`external/gstreamer-rockchip/gst/rockchipmpp/gstmppenc.c` 已核实）：
  - `rc-mode`：`"cbr"` / `"vbr"` / `"fixqp"`（默认 CBR）
  - `bps`：目标码率，单位 **bit/s**（0=auto，`rc:bps_target`）
  - `gop`：GOP 帧数（-1=跟随 fps）
  - `bps-min` / `bps-max`、`width` / `height`、`header-mode`（默认首帧含头）等

- 设备侧已实测可用的 v4l2→MPP 推流命令（用户提供）：
  ```
  v4l2src device=/dev/video-camera0 io-mode=mmap ! \
  video/x-raw,format=NV16,width=3840,height=2160,framerate=30/1 ! \
  mpph264enc ! h264parse ! rtph264pay pt=96
  ```
  关键结论：v4l2 节点为 `/dev/video-camera0`，输出 **NV16 3840×2160@30**（非 rockit 的 3864×2192），
  `v4l2src` 需 `io-mode=mmap`，`mpph264enc` 可直接接受 NV16 输入。`mpph265enc`/`mppjpegenc` 与
  `mpph264enc` 同源（`gstmppenc`），预期可用，需设备侧各验证一次。

### 3.4 构建系统（现状）

- 根 `Makefile`：`TARGET_PLATFORM=rv1126b make [debug] [install]`，build 目录
  `build/rv1126b-release|debug/`；`CMAKE_CFG` 目前只传 `CMAKE_TOOLCHAIN_FILE/TARGET_PLATFORM/
  CMAKE_BUILD_TYPE`。
- `toolchain-rv1126b.cmake`：编译器前缀 `aarch64-buildroot-linux-gnu-`（靠 PATH），sysroot 由
  `gcc --print-sysroot` 自动探测，`CMAKE_FIND_ROOT_PATH_MODE_{LIBRARY,INCLUDE,PACKAGE}=ONLY`。

## 4. 设计决策汇总

| 决策项 | 选择 | 理由 |
|---|---|---|
| 后端选择机制 | CMake 选项 `USE_GSTREAMER`（默认 OFF=rockit）+ 编译宏 `USE_GSTREAMER=1` | 用户指定；默认 OFF 不破坏现有 rockit 构建 |
| 代码结构 | 双后端类 + 宏分发工厂（方案①） | rockit 零改动、各自独立可测、无 `#ifdef` 汤 |
| GStreamer 管线 | `v4l2src(/dev/video-camera0, NV16 3840×2160) → tee → 3 分支 → mpp 编码器 → parse → appsink` | 单 sensor 源扇出，硬编，appsink 拉流复用现有消费链 |
| 帧粒度 | `h264parse/h265parse` + `stream-format=byte-stream,alignment=au` | 保证每 buffer=一帧 Annex-B，对齐 RTSP 拆 NAL |
| 关键帧判定 | `GST_BUFFER_FLAG_DELTA_UNIT`（无此 flag=IDR） | 与 parse 元素配合可靠，等价 rockit 的 IDR 映射 |
| MJPEG 路 | `mppjpegenc → appsink`（`image/jpeg`） | 无 NAL 语义，`is_idr()` 恒真，与 rockit 一致 |
| RTSP 层 | 保留现有 Live555 + StreamDistributor | 用户指定；改动最小 |
| 日志分类 | 新增 `GST`（`logger.h` 一行） | 与 `ROCKIT` 对称；zlog 通配规则自动覆盖 |
| 依赖库 | 按后端在 sysroot 内检测所需库，缺失即报错（见 §5.5） | 不硬编码 sysroot 路径，靠检测给出清晰报错 |

## 5. 详细设计

### 5.1 代码结构（`modules/platform/rv1126b/`）

```
modules/platform/rv1126b/
├── include/
│   ├── rv_video_pipeline.h       # rockit（保留，不改）
│   ├── rv_stream_fetcher.h       # rockit（保留，不改）
│   ├── gst_video_pipeline.h      # 新增
│   └── gst_stream_fetcher.h      # 新增
├── src/
│   ├── rv_video_pipeline.cpp     # rockit（保留，不改）
│   ├── rv_stream_fetcher.cpp     # rockit（保留，不改）
│   ├── gst_video_pipeline.cpp    # 新增
│   ├── gst_stream_fetcher.cpp    # 新增
│   └── rv_platform_factory.cpp   # 改：宏分发
└── CMakeLists.txt                # 改：按宏选源/库
```

两个后端类均置于命名空间 `rv1126bMedia`，与现有 `RvVideoPipeline` 一致。

### 5.2 工厂分发（`rv_platform_factory.cpp`）

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
    VencChannel chn, StreamType type, CodecType codec,
    StreamDistributor& distributor) {
#ifdef USE_GSTREAMER
    return std::unique_ptr<IStreamProvider>(
        new GstStreamFetcher(chn, type, codec, distributor));
#else
    return std::unique_ptr<IStreamProvider>(
        new RvStreamFetcher(chn, type, codec, distributor));
#endif
}
```

### 5.3 `GstVideoPipeline`（新增）

继承 `IVideoPipeline` + `IVideoEncoder`，单例（同 `RvVideoPipeline`）。

- `init()`：`gst_init`（如未初始化）→ `gst_parse_launch` 建管线 → 取三个 appsink 存 `app_sinks_[3]`
  → 检查管线是否可到 PAUSED（验证元素齐全）。
- `start()`：`gst_element_set_state(pipeline, GST_STATE_PLAYING)`。
- `stop()`：`gst_element_set_state(pipeline, GST_STATE_NULL)`。
- `deinit()`：`gst_object_unref(pipeline)`、`app_sinks_` 置空。
- `app_sink(VencChannel)`：返回该路 `GstAppSink*`，供 fetcher 拉流。
- `IVideoEncoder` 四个方法空实现（三路在 `init()` 固定建好，与 rockit 语义一致）。

管线字符串（常量，含注释）：

```
v4l2src device=/dev/video-camera0 io-mode=mmap ! \
  video/x-raw,format=NV16,width=3840,height=2160,framerate=30/1 ! tee name=t
t. ! queue ! mpph265enc rc-mode=cbr bps=8192000 gop=60 ! \
  h265parse ! video/x-h265,stream-format=byte-stream,alignment=au ! appsink name=app_main
t. ! queue ! videoscale ! video/x-raw,width=1280,height=720 ! \
  mpph264enc rc-mode=cbr bps=2048000 gop=60 ! \
  h264parse ! video/x-h264,stream-format=byte-stream,alignment=au ! appsink name=app_sub
t. ! queue ! videoscale ! video/x-raw,width=1280,height=720 ! \
  videorate ! video/x-raw,framerate=10/1 ! \
  mppjpegenc ! appsink name=app_mjpeg
```

appsink ↔ `VencChannel` 映射（`app_sink(channel)` 据此返回）：

| appsink 名 | VencChannel | StreamType | CodecType |
|---|---|---|---|
| `app_main` | CHN0 | VIDEO_MAIN | H265 |
| `app_sub` | CHN1 | VIDEO_SUB | H264 |
| `app_mjpeg` | CHN2 | VIDEO_MJPEG | MJPEG |

- `appsink` 关闭 signal 派发（`emit-signals=false`），改用 `gst_app_sink_pull_sample` 主动拉流。
- `queue` 用于 tee 分支解耦、避免跨分支阻塞。
- MJPEG 分支用 `videorate` 降到 10fps，与 rockit 的 `fr32DstFrameRateNum=10` 对齐。
- v4l2 节点 / NV16 / 分辨率 / 帧率做成常量（匿名命名空间），默认 `/dev/video-camera0`、NV16、
  3840×2160@30（来自用户实测命令）。
- 主码流分辨率 3840×2160（v4l2 暴露的 4K crop）与 rockit 主码流 3864×2192（sensor native 全读出）
  略有差异，属 v4l2 路径固有，接受之；如需严格一致另行在 capsfilter 裁剪。
- 子/MJPEG 分支 `videoscale` 会把 NV16 缩到 720p（必要时内部做格式转换）；若某编码器对 NV16 输入
  有兼容问题，则在该分支前加 `videoconvert ! video/x-raw,format=NV12` 兜底。

### 5.4 `GstStreamFetcher`（新增）

继承 `IStreamProvider`，结构对齐 `RvStreamFetcher`：

- 构造：保存 `channel_/stream_type_/codec_type_/distributor_`，并从
  `GstVideoPipeline::getInstance().app_sink(channel_)` 取 `GstAppSink*`。
- `start()/stop()`：起/停 `run()` 线程（`pthread_setname_np("GstFetch_<chn>")`）。
- `fetchFrame(chn, frame)`：
  1. `GstSample* s = gst_app_sink_pull_sample(sink_)`；超时（`try_pull_sample` 短超时）返回 -1，
     `run()` 里短暂 `usleep` 后重试（对齐 rockit 的 1000ms 超时+10ms 重试节奏）。
  2. 取 `GstBuffer`，`gst_buffer_map(..., GST_MAP_READ)` 拿指针与长度。
  3. `frame.pack_count=1`；`frame.seq` 自增；`frame.packs[0].data=buf`、`.len=size`、
     `.nalu_type = (buffer 无 GST_BUFFER_FLAG_DELTA_UNIT) ? IDR_SLICE : P_SLICE`
     （`MJPEG` 恒 `IDR_SLICE`）。
  4. 用 `FrameData` 组 `StreamFrame`（构造时深拷贝）→ `gst_sample_unref` / `unmap` → `push`。
- `releaseFrame()`：空实现（appsink sample 由 unref 归还，无单独 release 步骤）。

关键点：`StreamFrame` 深拷贝发生在 push 之前，随后立即 `unmap/unref`，不暴露 GStreamer 缓冲指针
越过 `StreamFrame` 构造（与 rockit 版 `RK_MPI_VENC_ReleaseStream` 顺序同构）。

### 5.5 构建（`CMakeLists.txt` + `Makefile`）

`modules/platform/rv1126b/CMakeLists.txt`：

```cmake
option(USE_GSTREAMER "Use GStreamer media backend (default: rockit)" OFF)

if(USE_GSTREAMER)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0)
    add_library(platform_impl STATIC
        src/gst_video_pipeline.cpp
        src/gst_stream_fetcher.cpp
        src/rv_platform_factory.cpp)
    target_include_directories(platform_impl PUBLIC
        include ${CMAKE_SOURCE_DIR}/modules/stream/include ${GST_INCLUDE_DIRS})
    target_link_libraries(platform_impl
        PUBLIC interface
        PRIVATE ${GST_LIBRARIES} pthread)
    target_compile_definitions(platform_impl PUBLIC PLATFORM_RV1126B=1 USE_GSTREAMER=1)
else()
    find_path(ROCKIT_INCLUDE_DIR rk_mpi_sys.h REQUIRED)
    find_library(ROCKIT_LIBRARY rockit REQUIRED)
    add_library(platform_impl STATIC
        src/rv_video_pipeline.cpp
        src/rv_stream_fetcher.cpp
        src/rv_platform_factory.cpp)
    target_include_directories(platform_impl PUBLIC
        include ${CMAKE_SOURCE_DIR}/modules/stream/include ${ROCKIT_INCLUDE_DIR})
    target_link_libraries(platform_impl PUBLIC interface PRIVATE ${ROCKIT_LIBRARY} pthread)
    target_compile_definitions(platform_impl PUBLIC PLATFORM_RV1126B=1)
endif()
```

根 `Makefile`：

- 新增 `USE_GSTREAMER ?= OFF`。
- `CMAKE_CFG` 追加 `-DUSE_GSTREAMER=$(USE_GSTREAMER)`。
- `help` 补充说明：`USE_GSTREAMER=ON TARGET_PLATFORM=rv1126b make`。

**依赖库检测（不硬编码 sysroot）**：`toolchain-rv1126b.cmake` 靠 `gcc --print-sysroot` 探测 sysroot，
构建时按所选后端在 sysroot 内检测所需库，缺失即 configure 失败并给出清晰报错：

- GStreamer（ON）：`pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-app-1.0)` —— 找不到直接
  报“缺少 GStreamer 开发库，请切换到含 GStreamer 的 SDK”。
- rockit（OFF）：`find_path(ROCKIT_INCLUDE_DIR rk_mpi_sys.h REQUIRED)` +
  `find_library(ROCKIT_LIBRARY rockit REQUIRED)` —— 找不到直接报“缺少 librockit，请切换到含 rockit 的 SDK”。

即：构建前只需让 PATH 中的 `aarch64-buildroot-linux-gnu-gcc` 对应所需 SDK，无需在 CMake/工具链里
写死任何绝对路径。`pkg-config` 交叉编译需确认 `PKG_CONFIG_SYSROOT_DIR`/`PKG_CONFIG_PATH` 解析到
sysroot 的 `.pc`（CMake 的 `pkg_check_modules` 配合 `CMAKE_SYSROOT` 处理，实现时验证）。

### 5.6 日志分类（`logger.h`）

`LOG_CATEGORY_LIST` 在 `X(ROCKIT)` 后加 `X(GST)`。`LogCategoryNames[]` 由 X-macro 自动扩展，
zlog 通配规则（`*.=INFO` 等）自动覆盖，无需改 `conf/zlog.conf`。GStreamer 后端日志统一用 `GST` 分类。

## 6. 隔离性保证

- rockit 后端文件零改动，`USE_GSTREAMER=OFF` 时编译产物与现状一致。
- GStreamer 新增文件仅在 `USE_GSTREAMER=ON` 时参与编译；`rv_platform_factory.cpp` 的宏分发保证
  同一平台目录只链接一套媒体库。
- `interface`/`core`/`stream`/`rtsp`/`app` 不改；海思平台（`hi3516cv610`）完全不编译本目录，零影响。
- `app/main.cpp` 的注册顺序（init → 三路 fetcher → RTSP main/sub）对两个后端通用，无需改动。

## 7. 风险与缓解

| 严重度 | 风险 | 缓解 |
|---|---|---|
| 中 | 子/MJPEG 分支缩放 + 格式转换路径未实测（源 NV16 3840×2160，仅主路 h264 实测通过） | 实现时用 `gst-launch-1.0` 逐分支串命令验证；必要时 `videoconvert` 兜底 |
| 中 | `videoscale` 两路 CPU 缩放（4K→720p）占用 CPU | 首版保底；后续换 ISP selfpath 第二路或 `rockchipmpp` 缩放 |
| 低 | mpp 编码器属性单位/取值（`bps` bit/s、`rc-mode` 字符串）与 MPP 版本耦合 | 已核实插件源码 + 用户实测 `mpph264enc` 可用；实现时先串命令验证再落码 |
| 低 | 依赖库检测失败时报错不够清晰 | `pkg_check_modules(REQUIRED)` / `find_path`+`find_library(REQUIRED)` 失败即停，报错写清缺哪套 SDK |
| 低 | `pkg-config` 交叉编译解析不到 sysroot `.pc` | 实现时验证，必要时设 `PKG_CONFIG_SYSROOT_DIR`/`PKG_CONFIG_PATH` |
| 低 | RTSP 起播花屏（需首帧含 SPS/PPS + IDR） | `h264parse/h265parse` 默认 header-mode=首帧含头，mpp 编码器默认同；与已修的 RTSP IDR-start 兼容 |

## 8. 待确认项

- **子/MJPEG 分支缩放与格式转换**：源 NV16 3840×2160 缩到 1280×720 是否需 `videoconvert` 兜底，
  实现时在设备上逐分支验证。
- **4K@30 硬编 + 两路缩放的实际负载**：设备侧 `perf top`/帧率观察（rockit 版 §8 已有同类项）。

## 9. 验证步骤

编译侧：

1. `TARGET_PLATFORM=rv1126b make`（默认 rockit）——确认与现状一致、rockit 路径无回归。
2. `USE_GSTREAMER=ON TARGET_PLATFORM=rv1126b make`——确认 GStreamer 后端可交叉编译、链接
   `libgstreamer-1.0`/`libgstapp-1.0`，产物 `readelf -d` 无 `librockit` 依赖。
3. `USE_GSTREAMER=ON ... make install`——确认安装目录含 eye 二进制与 conf。

设备侧（GStreamer 后端部署到 eye 固件）：

1. 运行 `/app/bin/eye`，确认 GStreamer 初始化日志（`GST` 分类）成功、三路 appsink 到位、管线 PLAYING。
2. RTSP 拉流 `ffprobe rtsp://<ip>:8554/main`、`/sub`：主码流 H265 3840×2160@30、子码流 H264 1280×720@30，
   起播无花屏。
3. `stream_test()` 产物 `/run/stream_chn0.h265`、`/run/stream_chn1.h264`、`/run/stream_chn2.mjpeg`
   可被 `ffprobe` 解析。
4. 持续运行 5~10 分钟，观察无 appsink 丢帧/编码器报错/内存增长。
5. `perf top -p $(pidof eye)` 观察 CPU（videoscale 负载见 §7）。

## 10. 附录

- rockit 现状：`modules/platform/rv1126b/src/rv_video_pipeline.cpp`、`rv_stream_fetcher.cpp`
- GStreamer 插件：`rv1126b_linux_build/external/gstreamer-rockchip/gst/rockchipmpp/`
- 编码器属性：`gstmppenc.c`（`rc-mode`/`bps`/`gop`）
- 相关既有设计：`docs/superpowers/specs/2026-07-05-rv1126b-port-design.md`、
  `docs/superpowers/specs/2026-07-28-rv1126b-imx415-adaptation-design.md`
