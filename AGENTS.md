# eye - 嵌入式视频处理应用

## 项目概述

基于海思 Hi3516CV610 平台的视频采集与编码应用，使用 ARM 交叉编译，运行在目标设备上。

- 传感器：SC4336P，最大分辨率 2560x1440@30fps
- 平台：Cortex-A7 + NEON，arm-v01c02-linux-musleabi 工具链
- C/C++ 标准：C11 / C++11

## 构建

```bash
cd build
./set.sh   # 清理 + cmake 配置（含 toolchain）
make       # 编译
```

`set.sh` 会调用 `clean.sh` 清理 build 目录后执行 cmake，toolchain 文件位于 `cmake/toolchain-arm-v01c02.cmake`。

安装目标默认 `~/eyeOut`（`cmake --install .` 或 `make install`）。

**不能在本地直接编译或运行**，所有编译必须通过交叉编译工具链。

## 架构

命名空间 `hiMppMedia`，所有主要类为单例模式。

| 模块 | 文件 | 职责 |
|------|------|------|
| 入口 | `source/main.cpp` | 初始化日志、视频处理，调用 `test_main()` |
| 视频处理 | `video_process_hi` | VI/VPSS/ISP 初始化与绑定，编码启动 |
| 视频编码 | `video_venc_hi` | H.265/H.264/MJPEG 编码参数配置 |
| 日志 | `logger` | zlog 封装，运行时配置路径 `/app/conf/zlog.conf` |
| 常量 | `global_constants` | 分辨率枚举 `ot_pic_size` 及尺寸数组 |

模块间数据流：Sensor → VI → VPSS（3 通道：原始/720p/算法）→ VENC

## 依赖

- **HiSilicon MPP**：`thirdparty/hi3516cv610_mpp/`（头文件 + 预编译库，ARM .so）
- **zlog**：`thirdparty/zlog/`（日志库，ARM .so）
- MPP 库列表见 `CMakeLists.txt` 中 `HI_MPP_LIB`

## 注意事项

- 海思 SDK 类型使用 `td_s32`、`ot_vi_pipe` 等前缀，非标准 POSIX 类型
- VPSS 三通道分辨率：Chn0=2560x1440, Chn1=1280x720, Chn2=640x384
- 日志宏用法：`LOGGER_INFO(HIMPP, "format %d", val)`，分类名对应 zlog category
- `test/` 目录用于测试代码，`test/stream/` 为测试流文件存放处
- `conf/zlog.conf` 为运行时日志配置，部署到设备 `/app/conf/`
