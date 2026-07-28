# RV1126B 平台 IMX415 适配设计

- 日期：2026-07-28
- 状态：已定稿，待实现
- 适用平台：RV1126B（瑞芯微 librockit）
- 影响范围：`modules/platform/rv1126b/`（仅此目录）

## 1. 背景与问题

eye 的 RV1126B 平台实现 `modules/platform/rv1126b/src/rv_video_pipeline.cpp` 中的硬编码参数
（分辨率/帧率/码率）最初照搬海思 Hi3516CV610 + SC4336P（4MP sensor），未针对实际板载的
**Sony IMX415（8MP sensor）** 适配。典型偏差：主码流常量 2688×1520（4MP 非标）、子码流 VGA、
码率偏低。

### 现状澄清

当前并非"不能用"：`VI_CHN_ATTR_S.stSize` 是 **ISP 输出尺寸**，sensor 实际按驱动默认输出
3864×2192（8MP），由 ISP 下采样兜底到 2688×1520。本任务是"规范化适配"。

## 2. 目标与非目标

### 目标

1. 主码流用满 IMX415 的 8MP native 输出（3864×2192@30fps，无 ISP 缩放）。
2. 子码流配套升到 720p。
3. 码率/GOP 等参数对齐 IMX415 能力。
4. 改动严格隔离在 RV1126B 平台目录内，不影响海思平台。

### 非目标

- 不引入配置化抽象/多 sensor profile（YAGNI）。
- 不处理 MJPEG 死代码（CHN2 无消费者），按用户决定保持。
- 不启用 HDR（保持线性 NO_HDR）。
- 不改 sensor 驱动、设备树、iqfile（系统层已正确）。

## 3. 关键事实依据

### 3.1 IMX415 native 模式表（4-lane，`imx415.c:1085-1304`）

| 分辨率 | bit | max_fps | hdr_mode | 备注 |
|---|---|---|---|---|
| 3864×2192 | 10 | 30 | NO_HDR | **mode[0]，驱动 probe 默认即此模式** |
| 3864×2192 | 10/12 | 30/20 | HDR_X2 / HDR_X3 | 不用 |
| 1944×1097 | 12 | 30 | NO_HDR/HDR_X2 | 不用 |
| 1284×720 | 12 | 90 | NO_HDR | 仅 2-lane，不用 |

IMX415 没有 4MP(2688×1520) native 模式。驱动默认选 mode[0]（4-lane + NO_HDR）。

### 3.2 官方示例 VI 初始化流程（决定性依据）

`external/rockit/mpi/example/mod/test_mpi_vi.cpp` 的 `test_vi_init()`（line 671-715）标准流程：

```
GetDevAttr → (若 NOT_CONFIG) SetDevAttr
            → (若未使能) EnableDev + SetDevBindPipe
SetChnAttr(仅设 stSize / enCompressMode / stIspOpt)
EnableChn
```

**全程不调用 `RK_MPI_VI_DevSelectSetting`，也不设 sensor 分辨率/mode**。`stChnAttr.stSize` 就是
ISP 输出尺寸，与 sensor native 解耦；sensor mode 由 rockit 内部按 v4l2 默认协商（= mode[0]，
正是本设计所需）。`eye` 当前 VI 配置流程与官方示例完全一致。

### 3.3 关键 API/结构体（已核实）

- `VI_SENSOR_MODE_E`（`rk_comm_vi.h:439-444`）：`SENSOR_NO_HDR=0`、`SENSOR_HDR_X2=5`、`SENSOR_HDR_X3=6`
- `VI_CHN_ATTR_S`（`rk_comm_vi.h:297-310`）：`stSize`(SIZE_S) / `enCompressMode` / `stIspOpt`(VI_ISP_OPT_S)
- `VI_ISP_OPT_S`（`:284-294`）：含 `stMaxSize`(isp output max resolution)、`u32BufCount`、`enMemoryType`
- `RK_FMT_YUV420SP`（`rk_comm_video.h:129`）、`RK_FMT_RGB_BAYER_SGBRG_10BPP`（`:192`）
- eye 现有 `set_vi_channel` / `create_venc_channel` 的结构体填充已与示例/头文件一致，无需改逻辑。

### 3.4 平台隔离机制

- `CMakeLists.txt:73`：`add_subdirectory(modules/platform/${TARGET_PLATFORM})` 按 `TARGET_PLATFORM`
  只编译一个平台目录。两平台各自编成同名静态库 `platform_impl`，互不可见。
- `interface`/`core`/`stream`/`app` 为平台无关层，本设计不改。

## 4. 设计决策汇总

| 决策项 | 选择 | 理由 |
|---|---|---|
| 主码流分辨率 | 8MP 3864×2192 | 用满 sensor，最高画质 |
| HDR | 线性 NO_HDR | iqfile 现有线性调校可直接复用 |
| 子码流分辨率 | 720p 1280×720 | 8MP 主码流的标准配套 |
| MJPEG 路 | 保持现状（接受随广播变 720p） | 用户决定；MJPEG 是死代码无消费者 |
| sensor mode 策略 | **跟随官方示例，靠 rockit 默认 mode[0]，不调 DevSelectSetting** | 与 test_mpi_vi.cpp 一致，风险最低，改动最小 |
| 参数组织 | 常量重定义 + 注释说明依据 | 自文档化，无大范围改名 |

> 修正记录（2026-07-28）：原方案曾考虑"显式锁定 sensor mode via DevSelectSetting"，核实官方
> 示例后改为"跟随官方示例"。`enSensorFmt`（`RK_FMT_RGB_BAYER_SGBRG_10BPP`）与
> `DevSelectSetting` 均不再使用，相应待确认项下线。

## 5. 详细设计

### 5.1 改动文件（仅 RV1126B 平台）

| 文件 | 改动 |
|---|---|
| `modules/platform/rv1126b/src/rv_video_pipeline.cpp` | 常量重定义（分辨率/码率）+ 常量块上方加注释说明对应 IMX415 mode[0]；VI/VENC 参数随常量自动生效 |

不改动：`rv_video_pipeline.h`、海思平台、`interface`、`core`、`stream`、`app`、sensor 驱动、设备树、iqfile。
不改 `init()` 调用链、不新增函数、不改 VI/VENC 结构体填充逻辑。

### 5.2 常量重定义（`rv_video_pipeline.cpp:17-34` 匿名命名空间）

| 常量 | 旧值（SC4336P） | 新值（IMX415） | 依据 |
|---|---|---|---|
| `kMainWidth` / `kMainHeight` | 2688 / 1520 | **3864 / 2192** | sensor mode[0] native，ISP 无缩放 |
| `kSubWidth` / `kSubHeight` | 640 / 480 | **1280 / 720** | ISP 下采样 |
| `kMjpegWidth` / `kMjpegHeight` | 640 / 480 | **1280 / 720** | 随 VI Chn2 广播，接受变 720p |
| `kFrameRate` | 30 | 30（不变） | mode[0] 支持 |
| `kMainBitrate` | 4096 | **8192** | 8MP H265（可调 8192~12288） |
| `kSubBitrate` | 1024 | **2048** | 720p H264 |
| `kMjpegBitrate` | 2048 | 2048（不变） | MJPEG 保持 |
| `kDevId/kPipeId/kVi*Chn/kVenc*Chn` | 0/0/3,2/0,1,2 | 不变 | 通道号与 sensor 无关 |

常量块上方加注释：`// 对应 IMX415 mode[0]: 3864x2192@30fps 10bit Linear，详见
// atk_dlrv1126b_linux6.1_sdk/kernel-6.1/drivers/media/i2c/imx415.c supported_modes[0]`

### 5.3 sensor mode 策略（跟随官方示例）

**不新增任何 sensor mode 设置代码。** sensor 输出由 rockit 按默认协商，即 imx415 驱动 mode[0]
（3864×2192@30fps 10bit Linear NO_HDR），与本设计目标一致。

依据：官方 `test_mpi_vi.cpp::test_vi_init()` 不调用 `DevSelectSetting`，仅设 VI channel 的
`stSize`（ISP 输出）。eye 沿用相同流程，主码流 `stSize=3864×2192` 时 ISP 无缩放、直接透传 sensor
native 输出。

### 5.4 VI channel（`set_vi_channel`，行 139-169）

逻辑不变，仅因常量改变而生效：
- VI Chn3（主）：`stSize = stMaxSize = 3864×2192`，`enPixelFormat = NV12` 不变
- VI Chn2（子）：`stSize = stMaxSize = 1280×720`（原 640×480）

VI Chn2 同时广播给 VENC CHN1(H264) 与 CHN2(MJPEG)，二者输入均为 720p。

### 5.5 VENC（`create_venc_channel`，行 171-213）

逻辑不变，仅因常量改变而生效：

| 路 | enType | RcMode | 分辨率 | dst/src帧率 | 码率(kbps) | GOP |
|---|---|---|---|---|---|---|
| CHN0 | HEVC | H265CBR | 3864×2192 | 30/30 | 8192 | 60 |
| CHN1 | AVC | H264CBR | 1280×720 | 30/30 | 2048 | 60 |
| CHN2 | MJPEG | MJPEGCBR | 1280×720 | 10/30 | 2048 | — |

`u32StreamBufCnt=4`、`u32BufSize=W*H`、`u32VirWidth=W` 保持。8MP 时 `u32BufSize` 显著增大，
若运行期出现 buffer 不足告警再调 `u32StreamBufCnt`。

## 6. 隔离性保证

- 改动文件仅属 `modules/platform/rv1126b/CMakeLists.txt`，海思构建（`TARGET_PLATFORM=hi3516cv610`）
  不编译这些文件。
- `interface` 层契约不变。海思平台 `modules/platform/hi3516cv610/` 零影响。

## 7. 风险与缓解

| 严重度 | 风险 | 缓解 |
|---|---|---|
| 高 | 8MP@30 H265 实时编码能力未核实（用户决定不预先验证） | 若设备实测编码器无法支撑，主码流回退 4MP(2560×1440)，仅改 `kMainWidth/Height/Bitrate` 三常量即可降级 |
| 低 | RTSP/网络带宽（约 10Mbps 总码流） | 链路可承载；如有压力可下调码率常量 |
| 低 | MJPEG 死代码仍占一个 fetcher 线程 | 不影响功能，本设计不处理 |

## 8. 实现期待确认项

- **8MP 编码负载**：设备 `perf top -p $(pidof eye)` 观察；不支撑则按 §7 高风险项降级。

（`enSensorFmt` / `DevSelectSetting` 两项因 sensor mode 策略修正已下线。）

## 9. 验证步骤（设备侧）

1. `TARGET_PLATFORM=rv1126b make` 交叉编译，部署到 `/app/bin/eye`。
2. 运行后用 `scripts/rk_dumpsys_compat.sh` 核对：sensor 实际输出 mode、VI chn 输出尺寸、
   VENC 三路分辨率/帧率。
3. RTSP 拉流（`ffprobe rtsp://<ip>:8554/main`、`/sub`）确认：主码流 3864×2192@30 H265、
   子码流 1280×720@30 H264。
4. `perf top -p $(pidof eye)` 观察编码器负载（验证 §7 高风险项）。
5. 持续运行 5~10 分钟，检查有无码流 buffer 不足/丢帧告警。

## 10. 附录

- IMX415 4-lane 完整模式表：`atk_dlrv1126b_linux6.1_sdk/kernel-6.1/drivers/media/i2c/imx415.c:1085-1304`
- 官方 VI 示例：`atk_dlrv1126b_linux6.1_sdk/external/rockit/mpi/example/mod/test_mpi_vi.cpp::test_vi_init`
- rockit MPI 头文件：`atk_dlrv1126b_linux6.1_sdk/external/rockit/mpi/sdk/include/`
