# eye RV1126B 项目映射

本文只描述当前仓库中 RV1126B 实现的拓扑和诊断映射。当前实现是 `VI -> VENC`，**没有 VPSS**。

## 实际拓扑

`rv_video_pipeline.cpp` 使用 VI device `0`、pipe `0`，启用两个 VI channel，并建立以下绑定：

| 流 | 输入 | 编码通道与参数 | 下游 |
|---|---|---|---|
| 主码流 | VI dev 0 / pipe 0 / ch 3 | VENC ch 0 / H.265 CBR / 2688x1520 / 30 fps / 4096 kbps | `VIDEO_MAIN`，RTSP `/main`，`/run/stream_chn0.h265` |
| 子码流 | VI dev 0 / pipe 0 / ch 2 | VENC ch 1 / H.264 CBR / 640x480 / 30 fps / 1024 kbps | `VIDEO_SUB`，RTSP `/sub`，`/run/stream_chn1.h264` |
| MJPEG | VI dev 0 / pipe 0 / ch 2 | VENC ch 2 / MJPEG CBR / 640x480 / 输入 30 fps、输出 10 fps / 2048 kbps | `VIDEO_MJPEG`，`/run/stream_chn2.mjpeg` |

绑定关系为：

- `VI ch3 -> VENC ch0`
- `VI ch2 -> VENC ch1`
- `VI ch2 -> VENC ch2`

因此子码流和 MJPEG 共享 VI ch2；主码流使用独立的 VI ch3。不存在可供检查的 VPSS 通道或 VPSS 计数器。

## 启动与取流

- `main()` 先初始化日志，再调用 `platform_video_pipeline().init()`；随后注册 VENC ch 0/1/2 对应的 `VIDEO_MAIN`、`VIDEO_SUB`、`VIDEO_MJPEG` 取流器。
- 管线初始化顺序是 `RK_MPI_SYS_Init` 成功后依次初始化 VI device、VI channels、VENC channels，最后绑定 VI 与 VENC。初始化失败会按已完成的阶段回收并调用 `RK_MPI_SYS_Exit`。
- 每个 VENC 通过 `RK_MPI_VENC_StartRecvFrame` 启动持续接收，`s32RecvPicNum = -1`。
- `RvStreamFetcher` 对所属 ch 通过 `RK_MPI_VENC_GetStream` 取流，超时参数为 `1000 ms`。失败只返回 `-1`，调用方休眠 `10 ms` 后重试；当前不会记录 `GetStream` 错误码。
- 取流成功后先创建 `StreamFrame`，把数据指针和长度复制到帧对象，再调用 `RK_MPI_VENC_ReleaseStream`。Release 失败会写 `STREAM` 错误日志，但帧仍会推送到分发器。
- `stream_test` 注册三个消费者并启动所有取流器，将帧写入三个 `/run` 文件。`/run` 是设备侧 tmpfs；文件增长只能证明下游收到了并写出了字节，不能单独证明图像内容正确。

## 进程、日志和输出路径

- 设备进程路径：`/app/bin/eye`。
- zlog 配置路径：`/app/conf/zlog.conf`；配置更新触发文件：`/app/conf/logupdate`。
- INFO 日志：`/tmp/eye_info.log`；WARN 及以上日志：`/tmp/eye_warn.log`。配置同时将各级日志输出到 stdout；上述文件位于 tmpfs，重启或断电后不保留。
- 日志分类包括 `HIMPP`、`TEST`、`STREAM`、`RTSP`。管线初始化和 Rockit MPI 错误主要使用 `HIMPP`，取流器使用 `STREAM`，测试写文件使用 `TEST`，RTSP 启停使用 `RTSP`。
- RTSP 服务端口为 `8554`，只挂载 `main` 和 `sub`；MJPEG 没有在 `main.cpp` 中注册 RTSP 路径。

## 症状到最小证据映射

| 症状 | 首选证据 | 判别重点 |
|---|---|---|
| 初始化失败 | eye 的 WARN/INFO/ERROR 日志 | 找到首个失败的 `RK_MPI_*` 调用及其错误码，再区分 VI device、VI channel、VENC channel 或 Bind 阶段；不要仅凭最终初始化失败判断根因。 |
| 所有流无数据 | `dumpsys sys`、`dumpsys vi`、`dumpsys venc`，以及 eye 日志 | 分别确认 VI 是否产帧、绑定计数是否增长、VENC 输入和编码输出是否增长；三者必须结合判断。 |
| 仅主码流异常 | VI ch3、VENC ch0、主流 `STREAM`/`TEST` 日志和 `/run/stream_chn0.h265` | 沿 `VI ch3 -> VENC ch0` 检查；不检查不存在的 VPSS。 |
| 子码流与 MJPEG 同时异常 | VI ch2、VENC ch1/ch2，以及对应 `STREAM`/`TEST` 日志和两个 `/run` 文件 | 两路共享 VI ch2，先检查共同输入，再分别核对两个 VENC 输出；不能只依据某一个输出计数器断言根因。 |
| `GetStream` 超时 | `dumpsys venc`、`dumpsys sys`、eye `STREAM` 日志 | 对照 VI 输入、绑定、VENC 编码输出以及 get/release 计数；代码对 GetStream 失败不记录错误码，因此日志中的重试现象不是错误码证据。 |
| 内存持续增长或卡死 | `dumpsys mb` 与 `dumpsys mb d` | 观察 block 数、长度、refs 和归属模块随时间的变化；单次快照或单个计数器不能单独证明泄漏或卡死根因。 |
| RTSP 无流但文件增长 | RTSP/STREAM 日志和 `/run` 文件计数 | 文件增长说明编码取流和测试写文件下游可能正常，应继续检查分发器、RTSP 注册/消费和网络路径；不能据此断言 Rockit 全链路无问题。 |

## RV1126B 与测试标签冲突

`test/stream/stream_test.cpp` 的 `stream_type_to_string()` 仍显示 HiSilicon 风格标签：

- `CHN0 H265 2560x1440`
- `CHN1 H264 1280x720`
- `CHN2 MJPEG 640x360`

这些字符串只用于测试结果日志显示，不是 RV1126B 管线配置。诊断 RV1126B 时必须以 `modules/platform/rv1126b/src/rv_video_pipeline.cpp` 的实际参数为准：主码流 `2688x1520`，子码流和 MJPEG `640x480`。不要把标签中的分辨率当作设备事实，也不要将 HiSilicon 专属 VPSS 拓扑套用到 RV1126B。

## 事实来源

- `modules/platform/rv1126b/src/rv_video_pipeline.cpp`：VI/VENC 参数、启动顺序、绑定和接收帧配置。
- `modules/platform/rv1126b/src/rv_stream_fetcher.cpp`：GetStream 超时、失败重试、深拷贝和 ReleaseStream 行为。
- `app/main.cpp`：进程入口中的三路取流器、RTSP 路径和端口。
- `test/stream/stream_test.cpp`：测试消费者、`/run` 输出文件和显示标签。
- `modules/core/include/logger.h`：日志分类、配置路径、更新触发路径和日志目录常量。
- `conf/zlog.conf`：stdout、INFO/WARN 文件输出和滚动配置。
- `AGENTS.md`：设备路径、`/run` tmpfs、日志持久性和 RV1126B 构建/运行环境约束。

## 可维护源码定位

以下行号按当前工作区源码核对，修改相关实现后应重新核对：

- `modules/platform/rv1126b/src/rv_video_pipeline.cpp:17-33`：VI/VENC 通道常量、分辨率、帧率和码率参数。
- `modules/platform/rv1126b/src/rv_video_pipeline.cpp:157-168`：VI ch3/ch2 两路 channel 初始化。
- `modules/platform/rv1126b/src/rv_video_pipeline.cpp:222-239`：VENC ch0/ch1/ch2 创建并启动接收。
- `modules/platform/rv1126b/src/rv_video_pipeline.cpp:272-278`：VI ch3/ch2 到 VENC ch0/ch1/ch2 的绑定。
- `modules/platform/rv1126b/src/rv_stream_fetcher.cpp:64-87`：`GetStream`、深拷贝、`ReleaseStream` 和分发行为。
- `modules/platform/rv1126b/src/rv_stream_fetcher.cpp:101-105`：取流失败后的 10 ms 重试。
- `app/main.cpp:21-35`：管线初始化及三路 `VencChannel`/`StreamType` 取流器映射。
- `modules/core/include/logger.h:121-122`：zlog 配置文件和更新触发文件路径。
- `conf/zlog.conf:49-50`：INFO/WARN 日志文件路径。
