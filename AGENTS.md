# eye 仓库指令

## 环境与构建

- 这是嵌入式视频采集/编码程序，通过 `TARGET_PLATFORM` 支持双平台：海思 Hi3516CV610（ARM Cortex-A7 32 位）与瑞芯微 RV1126B（aarch64）；不要尝试本地运行生成物，二进制运行在 ARM 目标设备上。
- Hi3516CV610 工具链前缀 `arm-v01c02-linux-musleabi-`，配置在 `cmake/toolchain-arm-v01c02.cmake`，CPU flags：`-mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4`。
- RV1126B 工具链前缀 `aarch64-buildroot-linux-gnu-`，配置在 `cmake/toolchain-rv1126b.cmake`，SDK root `/opt/aarch64-buildroot-linux-gnu_sdk-buildroot`，sysroot 设为 `${RV1126B_SDK_ROOT}/aarch64-buildroot-linux-gnu/sysroot`；未设专用 CPU flags（通用 aarch64）。
- CMake 必须显式带 toolchain；根 `CMakeLists.txt` 未设置 `CMAKE_TOOLCHAIN_FILE` 会直接 `FATAL_ERROR`。
- 标准构建顺序：`cd build && ./set.sh && make && cmake --install .`。
- `build/set.sh [hi3516cv610|rv1126b]` 按平台选择对应 toolchain（默认 `hi3516cv610`），会先执行 `build/clean.sh` 清掉 `build/` 内除 `.sh` 外的文件，再用 Release 配置 CMake。
- 默认安装目录是 `$HOME/eyeOut/${TARGET_PLATFORM}`（如 `$HOME/eyeOut/hi3516cv610`）；安装前根 `CMakeLists.txt` 会 `file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")`，清理只作用于当前平台子目录，不影响其他平台产物。不要把安装前缀指向需要保留的目录。
- `ENABLE_PEDANTIC=ON` 只额外加 `-Wpedantic`；没有独立 lint/format/test runner 配置。

## 结构与入口

- 可执行目标是 `eye`，入口在 `app/main.cpp`；启动顺序是日志初始化、`platform_video_pipeline().init()` 初始化平台管线、注册三路 `StreamFetcher`、启动 `RtspServer`（端口 8554，挂 `main`/`sub` 两路）、调用 `test_main()`、再 `pause()` 常驻。
- 根 CMake 固定添加 `modules/core`、`modules/interface`、`modules/stream`、`modules/platform/${TARGET_PLATFORM}`、`modules/rtsp`、`app`；默认 `TARGET_PLATFORM=hi3516cv610`。
- `modules/interface` 是接口库并链接 `core`，对外暴露平台工厂抽象 `platform_factory.h`（`platform_video_pipeline()` / `create_stream_fetcher()`，由各平台 `*_platform_factory.cpp` 实现）；`modules/stream` 是平台无关分发层；`modules/platform/hi3516cv610` 是 HiSilicon MPP 实现（链接 MPP/zlog/securec/pthread），`modules/platform/rv1126b` 走瑞芯微 `librockit`（aarch64，编译宏 `PLATFORM_RV1126B=1`）。
- `app/CMakeLists.txt` 当前把 `test/test.cpp` 和 `test/stream/stream_test.cpp` 编进正式 `eye` 目标；`test_main()` 不是单独测试二进制入口。

## 视频与码流事实（Hi3516CV610 专属，RV1126B 见 `modules/platform/rv1126b`，参数未核实）

- Sensor/VI 最大尺寸在代码中是 SC4336P `2560x1440@30fps`，MIPI 节点为 `/dev/ot_mipi_rx`。
- VPSS 三个物理通道在 `hi_video_pipeline.h`/`hi_video_pipeline.cpp`：Chn0 `2560x1440@30`，Chn1 `1280x720@30`，Chn2 `640x384` 且 dst frame rate 为 `5`。
- VENC 三路在 `venc_set_video_param()`：Chn0 H.265 CBR `PIC_2560X1440@30`，Chn1 H.264 CBR `PIC_720P@30`，Chn2 MJPEG CBR `PIC_360P@10`。不要把 VPSS Chn2 的 `640x384` 误写成 MJPEG 输出分辨率。
- `StreamConsumerManager` 在全局命名空间，不在 `hiMppMedia`；构造时固定为 `VIDEO_MAIN/SUB/MJPEG` 建三个 `StreamDistributor`。
- `VencChannel`、`StreamType`、`CodecType` 定义在 `modules/core/include/common_types.h` 的全局作用域。

## 运行时路径与日志

- 设备侧（Hi3516CV610）当前系统为 ARMv7 Linux `5.10.221` + BusyBox `1.34.1`；`/app` 挂载自 `/dev/ubi1_0`，`/data` 挂载自 `/dev/ubi2_0`，`/run` 是 tmpfs。RV1126B 设备侧系统/挂载信息待核实。
- 设备侧 `eye` 部署并运行在 `/app/bin/eye`；Telnet 验证时进程形态为 `/app/bin/eye`。
- zlog 配置硬编码为 `/app/conf/zlog.conf`，更新触发文件是 `/app/conf/logupdate`；INFO/WARN 日志写到 tmpfs `/tmp/eye_info.log`、`/tmp/eye_warn.log`，重启或断电后不保留。
- `conf/` 安装到安装前缀的 `conf/`；部署到设备时要保证 `/app/conf/zlog.conf` 存在。
- 日志分类定义在 `logger.h` 的 `LOG_CATEGORY_LIST`：`HIMPP`、`TEST`、`STREAM`、`RTSP`；宏用法是 `LOGGER_INFO(HIMPP, "format %d", value)`。
- `stream_test()` 写设备侧 tmpfs 文件：`/run/stream_chn0.h265`、`/run/stream_chn1.h264`、`/run/stream_chn2.mjpeg`，重启或清理 `/run` 后不会保留。

## 第三方与安装

- Hi3516CV610 的 MPP 头/库来自 `thirdparty/hi3516cv610_mpp/`，zlog 来自 `thirdparty/zlog/`；RV1126B 的 `librockit` 等运行时库来自交叉编译 sysroot（`toolchain-rv1126b.cmake` 设的 `CMAKE_SYSROOT`），不在仓库 `thirdparty` 内。
- Hi3516CV610 构建时安装阶段会复制 `thirdparty` 下除 `hi3516cv610_mpp`/`rv1126b_mpp` 目录外的 `.so*` 到 `lib/`；RV1126B 构建时 `.so` 列表置空，运行时库由 SDK rootfs 提供，安装不复制。MPP `.so` 不会被安装规则复制。
