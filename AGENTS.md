# eye 仓库指令

## 环境与构建

- 这是海思 Hi3516CV610 嵌入式视频采集/编码程序；不要尝试本地运行生成物，二进制运行在 ARM 目标设备上。
- 交叉工具链前缀是 `arm-v01c02-linux-musleabi-`，CPU flags 在 `cmake/toolchain-arm-v01c02.cmake`：`-mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4`。
- CMake 必须显式带 toolchain；根 `CMakeLists.txt` 未设置 `CMAKE_TOOLCHAIN_FILE` 会直接 `FATAL_ERROR`。
- 标准构建顺序：`cd build && ./set.sh && make && cmake --install .`。
- `build/set.sh` 会先执行 `build/clean.sh`，清掉 `build/` 内除 `.sh` 外的文件，再用 Release 配置 CMake。
- 默认安装目录是 `$HOME/eyeOut`；安装前根 `CMakeLists.txt` 会 `file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")`，不要把安装前缀指向需要保留的目录。
- `ENABLE_PEDANTIC=ON` 只额外加 `-Wpedantic`；没有独立 lint/format/test runner 配置。

## 结构与入口

- 可执行目标是 `eye`，入口在 `app/main.cpp`；启动顺序是日志初始化、`videoProcessHi::init()` 初始化 MPP 管线、注册三路 `StreamFetcher`、调用 `test_main()`、再 `pause()` 常驻。
- 根 CMake 固定添加 `modules/core`、`modules/interface`、`modules/stream`、`modules/platform/${TARGET_PLATFORM}`、`app`；默认 `TARGET_PLATFORM=hi3516cv610`。
- `modules/interface` 只是接口库并链接 `core`；`modules/stream` 是平台无关分发层；`modules/platform/hi3516cv610` 是 HiSilicon MPP 实现并链接 MPP/zlog/securec/pthread。
- `app/CMakeLists.txt` 当前把 `test/test.cpp` 和 `test/stream/stream_test.cpp` 编进正式 `eye` 目标；`test_main()` 不是单独测试二进制入口。

## 视频与码流事实

- Sensor/VI 最大尺寸在代码中是 SC4336P `2560x1440@30fps`，MIPI 节点为 `/dev/ot_mipi_rx`。
- VPSS 三个物理通道在 `hi_video_pipeline.h`/`hi_video_pipeline.cpp`：Chn0 `2560x1440@30`，Chn1 `1280x720@30`，Chn2 `640x384` 且 dst frame rate 为 `5`。
- VENC 三路在 `venc_set_video_param()`：Chn0 H.265 CBR `PIC_2560X1440@30`，Chn1 H.264 CBR `PIC_720P@30`，Chn2 MJPEG CBR `PIC_360P@10`。不要把 VPSS Chn2 的 `640x384` 误写成 MJPEG 输出分辨率。
- `StreamConsumerManager` 在全局命名空间，不在 `hiMppMedia`；构造时固定为 `VIDEO_MAIN/SUB/MJPEG` 建三个 `StreamDistributor`。
- `VencChannel`、`StreamType`、`CodecType` 定义在 `modules/core/include/common_types.h` 的全局作用域。

## 运行时路径与日志

- zlog 配置硬编码为 `/app/conf/zlog.conf`，更新触发文件是 `/app/conf/logupdate`，日志目录是 `/data/eyeLog`。
- `conf/` 安装到安装前缀的 `conf/`；部署到设备时要保证 `/app/conf/zlog.conf` 存在。
- 日志分类只定义了 `HIMPP`、`TEST`、`STREAM`；宏用法是 `LOGGER_INFO(HIMPP, "format %d", value)`。
- `stream_test()` 写设备侧文件：`/run/stream_chn0.h265`、`/run/stream_chn1.h264`、`/run/stream_chn2.mjpeg`。

## 第三方与安装

- MPP 头/库来自 `thirdparty/hi3516cv610_mpp/`，zlog 来自 `thirdparty/zlog/`。
- 安装阶段会复制 `thirdparty` 下除 `hi3516cv610_mpp` 目录外的 `.so*` 到 `lib/`；MPP `.so` 不会被安装规则复制。
