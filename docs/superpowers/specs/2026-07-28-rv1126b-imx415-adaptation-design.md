# RV1126B 平台 IMX415 适配设计

- 日期：2026-07-28
- 状态：已定稿，待实现
- 适用平台：RV1126B（瑞芯微 librockit）
- 影响范围：`modules/platform/rv1126b/`（仅此目录）

## 1. 背景与问题

eye 的 RV1126B 平台实现 `modules/platform/rv1126b/src/rv_video_pipeline.cpp` 中的硬编码参数
（分辨率/帧率/码率）最初是照搬海思 Hi3516CV610 + SC4336P（4MP sensor）的配置写出的，
没有针对实际板载的 **Sony IMX415（8MP sensor）** 进行适配。

典型偏差：

- 主码流常量 `kMainWidth=2688 / kMainHeight=1520`（约 4MP 非标尺寸）与 IMX415 能力不符。
- 子码流 640×480(VGA) 与 8MP 主码流档位差距过大。
- 码率（主 4096 kbps）对 8MP H265 偏低。
- 未显式设置 sensor 输出模式，依赖驱动默认行为。

### 现状澄清

当前代码并非"不能用"：`VI_CHN_ATTR_S.stSize` 是 **ISP 输出尺寸**，sensor 实际按驱动默认输出
3864×2192（8MP），由 ISP 下采样兜底到 2688×1520。因此本任务是"规范化适配"，不是修复崩溃。

## 2. 目标与非目标

### 目标

1. 主码流用满 IMX415 的 8MP native 输出（3864×2192@30fps，无 ISP 缩放）。
2. 子码流配套升到 720p。
3. 显式锁定 sensor mode，不再依赖驱动默认这一隐式约定。
4. 码率/GOP 等参数对齐 IMX415 能力。
5. 改动严格隔离在 RV1126B 平台目录内，不影响海思平台。

### 非目标

- 不引入配置化抽象/多 sensor profile（YAGNI，当前仅一种 sensor）。
- 不处理 MJPEG 死代码（CHN2 无消费者），按用户决定保持现状。
- 不启用 HDR（保持线性 NO_HDR）。
- 不改 sensor 驱动、设备树、iqfile（系统层已正确）。

## 3. 关键事实依据

### 3.1 IMX415 native 模式表（4-lane，摘自 `imx415.c:1085-1304`）

| 分辨率 | bit | max_fps | hdr_mode | 备注 |
|---|---|---|---|---|
| 3864×2192 | 10 | 30 | NO_HDR | **mode[0]，本设计选用** |
| 3864×2192 | 10/12 | 30/20 | HDR_X2 / HDR_X3 | 不用 |
| 1944×1097 | 12 | 30 | NO_HDR/HDR_X2 | 2MP binning，不用 |
| 1284×720 | 12 | 90 | NO_HDR | 仅 2-lane，不用 |

- IMX415 **没有 4MP(2688×1520) native 模式**；4MP 只能靠 ISP 缩放产生。
- 驱动 probe 默认选 mode[0]（4-lane + NO_HDR，`imx415.c:3228-3233`）。

### 3.2 rockit sensor mode 设置 API

- `RK_MPI_VI_DevSelectSetting(VI_DEV, VI_SENSOR_SETTING_S*)`（`rk_mpi_vi.h:98`）
- `VI_SENSOR_SETTING_S`（`rk_comm_vi.h:446-452`）：`u32SensorWidth/Height/Fps`、
  `enSensorFmt`、`enSensorMode`（LINEAR/HDR2X/HDR3X）

### 3.3 平台隔离机制

- `CMakeLists.txt:73`：`add_subdirectory(modules/platform/${TARGET_PLATFORM})` 按 `TARGET_PLATFORM`
  只编译一个平台目录。
- 两个平台各自编成同名静态库 `platform_impl`，CMake 二选一，互不可见。
- `interface`/`core`/`stream`/`app` 为平台无关层，本设计不改。

## 4. 设计决策汇总

| 决策项 | 选择 | 理由 |
|---|---|---|
| 主码流分辨率 | 8MP 3864×2192 | 用满 sensor，最高画质 |
| HDR | 线性 NO_HDR | iqfile 现有线性调校可直接复用，工作量小 |
| 子码流分辨率 | 720p 1280×720 | 8MP 主码流的标准配套 |
| MJPEG 路 | 保持现状（接受随广播变 720p） | 用户决定；MJPEG 是死代码无消费者 |
| sensor mode 处理 | 显式锁定 mode[0] | 行为确定，不依赖驱动默认 |
| 实现策略 | 显式锁定 + 参数规范化 | 自文档化，换驱动不变 |

## 5. 详细设计

### 5.1 改动文件（仅 RV1126B 平台）

| 文件 | 改动 |
|---|---|
| `modules/platform/rv1126b/src/rv_video_pipeline.cpp` | 常量重定义；新增 `init_vi_sensor_mode()`；VI/VENC 参数更新；`init()` 调用链与 goto 回滚顺序同步 |
| `modules/platform/rv1126b/include/rv_video_pipeline.h` | 新增 `init_vi_sensor_mode()` 私有声明 |

不改动：海思平台、`interface`、`core`、`stream`、`app`、sensor 驱动、设备树、iqfile。

### 5.2 常量重定义（`rv_video_pipeline.cpp:17-34` 匿名命名空间）

| 常量 | 旧值 | 新值 | 依据 |
|---|---|---|---|
| `kMainWidth` / `kMainHeight` | 2688 / 1520 | **3864 / 2192** | sensor mode[0] native |
| `kSubWidth` / `kSubHeight` | 640 / 480 | **1280 / 720** | ISP 下采样 |
| `kMjpegWidth` / `kMjpegHeight` | 640 / 480 | **1280 / 720** | 随 VI Chn2 广播，接受变 720p |
| `kFrameRate` | 30 | 30（不变） | mode[0] 支持 |
| `kMainBitrate` | 4096 | **8192** | 8MP H265（可调 8192~12288） |
| `kSubBitrate` | 1024 | **2048** | 720p H264 |
| `kMjpegBitrate` | 2048 | 2048（不变） | MJPEG 保持 |
| `kDevId/kPipeId/kVi*Chn/kVenc*Chn` | 0/0/3,2/0,1,2 | 不变 | 通道号与 sensor 无关 |

参数规范化方式：保留现有命名风格，仅在常量块上方加注释说明"对应 IMX415 mode[0]，详见
atk_dlrv1126b_linux6.1_sdk/kernel-6.1/drivers/media/i2c/imx415.c supported_modes[0]"，不做大范围改名。

### 5.3 sensor mode 显式锁定（新增 `init_vi_sensor_mode()`）

在 `init()` 调用链插入新一步，位置在 `init_vi_dev()` 之后、`init_vi_channels()` 之前：

```
init()
 ├─ init_sys()
 ├─ init_vi_dev()
 ├─ init_vi_sensor_mode()    ← 新增
 ├─ init_vi_channels()
 ├─ init_venc_channels()
 └─ bind_channels()
```

逻辑（示意）：

```cpp
static int init_vi_sensor_mode() {
    VI_SENSOR_SETTING_S s{};
    s.u32SensorWidth  = 3864;
    s.u32SensorHeight = 2192;
    s.u32SensorFps    = 30;
    s.enSensorFmt     = <IMX415 mode[0] SGBRG10 raw 对应的 PIXEL_FORMAT_E>;  // 见 §8
    s.enSensorMode    = VI_SENSOR_MODE_LINEAR;
    return RK_MPI_VI_DevSelectSetting(kDevId, &s);
}
```

`init()` 的 goto 错误回滚顺序同步调整（sensor mode 锁定无显式反操作，但 goto 标签位置一致）。

### 5.4 VI channel 配置（`set_vi_channel`，行 139-169）

- VI Chn3（主）：`stSize = stMaxSize = 3864×2192`，`enPixelFormat = NV12` 不变
- VI Chn2（子）：`stSize = stMaxSize = 1280×720`（原 640×480）

VI Chn2 同时广播给 VENC CHN1(H264) 与 CHN2(MJPEG)，二者输入均为 720p；MJPEG 的 640×480 不再保留。

### 5.5 VENC 配置（`create_venc_channel`，行 171-213）

| 路 | enType | RcMode | 分辨率 | dst/src帧率 | 码率(kbps) | GOP |
|---|---|---|---|---|---|---|
| CHN0 | HEVC | H265CBR | 3864×2192 | 30/30 | 8192 | 60 |
| CHN1 | AVC | H264CBR | 1280×720 | 30/30 | 2048 | 60 |
| CHN2 | MJPEG | MJPEGCBR | 1280×720 | 10/30 | 2048 | — |

`u32StreamBufCnt=4`、`u32BufSize=W*H`、`u32VirWidth=W` 保持。8MP 时 `u32BufSize` 显著增大，
需确认码流 buffer 池充足（若运行期出现 buffer 不足告警再调 `u32StreamBufCnt`）。

## 6. 隔离性保证

- 改动文件仅属 `modules/platform/rv1126b/CMakeLists.txt`，海思构建（`TARGET_PLATFORM=hi3516cv610`）
  不会编译这些文件。
- `interface` 层契约（`platform_factory.h`、`IVideoPipeline`、`IStreamProvider`）不变。
- 海思平台 `modules/platform/hi3516cv610/` 零影响。

## 7. 风险与缓解

| 严重度 | 风险 | 缓解 |
|---|---|---|
| 高 | 8MP@30 H265 实时编码能力未核实（用户决定不预先验证） | 若设备实测编码器无法支撑，主码流回退到 4MP(2560×1440)，仅改 `kMainWidth/Height/Bitrate` 三常量即可降级 |
| 中 | `RK_MPI_VI_DevSelectSetting` 在本 SDK 是否真生效（闭源 librockit.so） | 回退：直接打开 sensor v4l-subdev 节点发 `VIDIOC_SUBDEV_S_FMT`；或保留驱动默认 + 注释说明 |
| 中 | `enSensorFmt` 枚举名待确认 | 实现时查 `rk_comm_video.h` 确定 SGBRG10 对应枚举 |
| 低 | RTSP/网络带宽（约 10Mbps 总码流） | 链路可承载；如有压力可下调码率常量 |
| 低 | MJPEG 死代码仍占一个 fetcher 线程 | 不影响功能，本设计不处理 |

## 8. 实现期待确认项

1. **`enSensorFmt` 枚举值**：IMX415 mode[0] 输出 `MEDIA_BUS_FMT_SGBRG10_1X10` raw。
   rockit `PIXEL_FORMAT_E`（`rk_comm_video.h`）中对应枚举名需在实现时确认。
2. **`RK_MPI_VI_DevSelectSetting` 生效性**：设备实测其返回值；不生效则走 §7 回退。
3. **8MP 编码负载**：设备 `perf top` 观察；不支撑则按 §7 高风险项降级。

## 9. 验证步骤（设备侧）

1. `TARGET_PLATFORM=rv1126b make` 交叉编译，部署到 `/app/bin/eye`。
2. 运行后用 `scripts/rk_dumpsys_compat.sh` 核对：sensor 实际输出 mode、VI chn 输出尺寸、
   VENC 三路分辨率/帧率。
3. RTSP 拉流（`ffprobe rtsp://<ip>:8554/main`、`/sub`）确认：主码流 3864×2192@30 H265、
   子码流 1280×720@30 H264。
4. `perf top -p $(pidof eye)` 观察编码器负载（验证 §7 高风险项）。
5. 持续运行 5~10 分钟，检查有无码流 buffer 不足/丢帧告警。

## 10. 附录：IMX415 4-lane 完整模式表

详见 `atk_dlrv1126b_linux6.1_sdk/kernel-6.1/drivers/media/i2c/imx415.c:1085-1304`。
本设计仅使用 mode[0]（3864×2192@30fps 10bit Linear）。
