# eye - 嵌入式视频处理应用

## 项目概述

基于海思 Hi3516CV610 平台的视频采集与编码应用，ARM 交叉编译，运行在目标设备上。

- 传感器：SC4336P，最大分辨率 2560x1440@30fps
- 平台：Cortex-A7 + NEON，arm-v01c02-linux-musleabi 工具链
- 标准：C11 / C++11

## 构建

```bash
cd build
./set.sh   # 清理 + cmake 配置（含 toolchain）
make       # 编译
cmake --install .   # 安装到 ~/eyeOut（默认）
```

- `set.sh` 调用 `clean.sh` 清理 build 目录后执行 cmake
- toolchain 文件：`cmake/toolchain-arm-v01c02.cmake`
- **不能本地编译或运行**，所有编译必须通过交叉编译工具链
- CMake 要求显式指定 `-DCMAKE_TOOLCHAIN_FILE`，否则报错
- `ENABLE_PEDANTIC` 选项可开启更严格的警告

## 架构

模块化分层设计，根 `CMakeLists.txt` 通过 `add_subdirectory` 管理：

```
app/                 → 可执行文件入口 (eye)
modules/
  interface/         → 抽象接口：IVideoPipeline、IVideoEncoder、IStreamProvider
  core/              → 日志 (logger)、公共类型 (common_types)、全局常量
  stream/            → 平台无关的码流处理：Frame、Distributor、ConsumerManager
  platform/hi3516cv610/  → 海思平台实现：pipeline、venc 参数、stream fetcher
```

| 模块 | 关键文件 | 职责 |
|------|----------|------|
| 入口 | `app/main.cpp` | 初始化日志、pipeline、三路码流 Fetcher，调用 `test_main()` |
| 平台实现 | `hi_video_pipeline.cpp` | VI/VPSS/ISP/VENC 初始化与绑定 |
| 编码参数 | `hi_venc_param.cpp` | H.265/H.264/MJPEG 通道参数配置 |
| 码流获取 | `hi_stream_fetcher.cpp` | 从 VENC 获取编码帧 |
| 码流分发 | `stream_distributor.cpp` | 将帧分发给已注册的 Consumer |
| 消费者管理 | `stream_consumer_manager.cpp` | 全局单例，管理 Fetcher 与 Distributor 生命周期 |
| 日志 | `modules/core/src/logger.cpp` | zlog 封装，运行时配置 `/app/conf/zlog.conf` |

数据流：
```
Sensor → MIPI_RX → VI(PIPE) → ISP → VI(CHN) → VPSS → VENC → StreamFetcher → StreamDistributor → Consumers
                                    (RAW)   (RGB→YUV)                    ├→ VO
                                                                          ├→ VENC（Chn0: 2560x1440 H.265 / Chn1: 1280x720 H.264 / Chn2: 640x384 MJPEG）
                                                                          └→ USER
```

## 依赖

- **HiSilicon MPP**：`thirdparty/hi3516cv610_mpp/`（头文件 + ARM .so）
- **zlog**：`thirdparty/zlog/`（头文件 + ARM .so）
- MPP 库列表见根 `CMakeLists.txt` 中 `HI_MPP_LIB`

## 注意事项

- 海思 SDK 类型使用 `td_s32`、`ot_vi_pipe` 等前缀，非标准 POSIX 类型
- VPSS 三通道分辨率：Chn0=2560x1440, Chn1=1280x720, Chn2=640x384
- 日志宏用法：`LOGGER_INFO(HIMPP, "format %d", val)`，分类名对应 zlog category；可用分类：HIMPP、TEST、STREAM
- `common_types.h` 中 `VencChannel`/`StreamType`/`CodecType` 等枚举在全局作用域（非 `hiMppMedia` 命名空间）
- `StreamConsumerManager` 是全局单例（非 `hiMppMedia` 命名空间），通过 `register_consumer()` 注册回调消费码流
- `test/` 目录用于测试代码，`test_main()` 为测试入口；`test/stream/` 为测试流文件存放处
- `conf/zlog.conf` 为运行时日志配置，部署到设备 `/app/conf/`
- 安装时自动清理目标目录，并拷贝 `thirdparty` 中除 MPP 外的 `.so` 到 `lib/`
