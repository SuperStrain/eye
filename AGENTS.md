# eye 仓库指令

## 环境与构建

- 这是嵌入式视频采集/编码程序，通过 `TARGET_PLATFORM` 支持双平台：海思 Hi3516CV610（ARM Cortex-A7 32 位）与瑞芯微 RV1126B（aarch64）；不要尝试本地运行生成物，二进制运行在 ARM 目标设备上。
- Hi3516CV610 工具链前缀 `arm-v01c02-linux-musleabi-`，配置在 `cmake/toolchain-arm-v01c02.cmake`，CPU flags：`-mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4`。
- RV1126B 工具链前缀 `aarch64-buildroot-linux-gnu-`，配置在 `cmake/toolchain-rv1126b.cmake`；该文件不硬编码任何绝对路径——编译器只用前缀（依赖 PATH 查找工具链 bin），`CMAKE_SYSROOT` 通过 `gcc --print-sysroot` 自动探测（可用 `-DCMAKE_SYSROOT=...` 手动覆盖）；未设专用 CPU flags（通用 aarch64）。构建前提是 PATH 已包含工具链 bin 目录。
- CMake 必须显式带 toolchain；根 `CMakeLists.txt` 未设置 `CMAKE_TOOLCHAIN_FILE` 会直接 `FATAL_ERROR`。
- 标准构建顺序：`make`（默认 Release，`-O2`）或 `make debug`（`-O0 -g`，perf/gdb 调试用），再 `make install`；构建入口是仓库根 `Makefile`。
- `Makefile` 封装 cmake 配置/编译/安装，构建目录按「平台-类型」分离：`build/hi3516cv610-release/`、`build/hi3516cv610-debug/`、`build/rv1126b-release/`、`build/rv1126b-debug/`；支持 `TARGET_PLATFORM=rv1126b make [debug]` 切换平台。`make clean` 清当前配置目录，`make distclean` 清所有 build 目录。
- Release 与 Debug 均保留符号表（`CMakeLists.txt` 不再用链接 `-s` strip），便于设备侧 `perf top -p` 解析函数名；如需最小体积可手动 `arm-v01c02-linux-musleabi-strip eye`。
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

- Hi3516CV610 的 MPP 头/库来自 `thirdparty/hi3516cv610_mpp/`，zlog 来自 `thirdparty/zlog/`；RV1126B 的 `librockit` 等运行时库来自交叉编译 sysroot（`toolchain-rv1126b.cmake` 自动探测的 `CMAKE_SYSROOT`），不在仓库 `thirdparty` 内。
- Hi3516CV610 构建时安装阶段会复制 `thirdparty` 下除 `hi3516cv610_mpp`/`rv1126b_mpp` 目录外的 `.so*` 到 `lib/`；RV1126B 构建时 `.so` 列表置空，运行时库由 SDK rootfs 提供，安装不复制。MPP `.so` 不会被安装规则复制。

## RV1126B dumpsys 已知问题

- SDK 内 `external/rockit`（用户态 `librockit.so` + `dumpsys`，Dec 15 / `git-5057bd373`）与 `external/ipc_drv_ko`（内核 `rockit-ko`，Dec 30 / `v2.48.0`）版本错配 15 天；DumpSys 系列 ioctl 协议在此期间演进，导致 `/usr/bin/dumpsys` 经由 eye 内置 `ipcs_server`（TCP `127.0.0.1:3893`）查询内核状态时全部返回空。表现为 `dumpsys venc` 输出仅头/尾、`dumpsys sys` 绑定表为空、`dumpsys vi` 计数器全为 0。eye 主体功能（VI/VENC/Bind/GetStream）不受影响。
- 临时替代工具：`scripts/rk_dumpsys_compat.sh`，直接读 `/dev/mpi/vsys`、`/dev/mpi/valloc`、`/dev/mpi/venc`，可显示绑定关系、节点帧计数、MB 分配、连续 fps 采样。详见 `docs/rv1126b-dumpsys-blank-output.md`。
- 根修复需向 Rockchip/Alientek 索取与 `external/ipc_drv_ko` 同源（commit `9ca741946f...`，v2.48.0）的 `librockit.so` + `dumpsys`，替换后 `dumpsys venc` 应能完整输出。未拿到匹配二进制前，不要替换 `/usr/lib/librockit.so` 或降级 `/usr/lib/module/rockit*.ko`，会破坏 eye 当前正常工作的视频流。
