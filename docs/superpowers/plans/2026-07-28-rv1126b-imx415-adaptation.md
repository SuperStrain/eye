# RV1126B IMX415 适配 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 RV1126B 平台视频流水线硬编码参数从海思 SC4336P（4MP）适配为 IMX415（8MP），主码流用满 sensor native 输出。

**Architecture:** 仅修改 `rv_video_pipeline.cpp` 匿名命名空间内的常量块（分辨率/码率）并加注释。VI channel 与 VENC 的结构体填充逻辑不变，参数随常量自动生效。sensor mode 跟随官方 `test_mpi_vi.cpp` 示例，由 rockit 默认协商为 imx415 mode[0]（3864×2192@30fps NO_HDR），不调用 `DevSelectSetting`。

**Tech Stack:** C++11 / 瑞芯微 librockit MPI（`RK_MPI_*`）/ CMake 交叉编译（aarch64-buildroot-linux-gnu-）。

## Global Constraints

- **平台隔离**：仅改 `modules/platform/rv1126b/`，不得影响海思平台。根 `CMakeLists.txt:73` 按 `TARGET_PLATFORM` 条件编译，海思构建不触碰 RV1126B 文件。
- **构建**：交叉编译，入口 `TARGET_PLATFORM=rv1126b make`（根 Makefile 封装 cmake）。二进制运行在 ARM 目标设备，本地不可执行。
- **无主机测试**：本项目无 test runner（AGENTS.md），验证策略 = 交叉编译通过 + 设备侧 dumpsys/RTSP ffprobe 功能验证。
- **不改系统层**：不改 sensor 驱动、设备树、iqfile（系统层已正确识别 IMX415）。
- **代码风格**：不添加注释除非必要（本任务注释用于说明参数依据，属必要）；保持现有命名风格。

## File Structure

| 文件 | 责任 | 本计划改动 |
|---|---|---|
| `modules/platform/rv1126b/src/rv_video_pipeline.cpp` | RV1126B 视频流水线（VI/VENC 初始化、绑定） | 修改匿名命名空间常量块（行 23-33）+ 加依据注释 |

不创建新文件。不改 `.h`、不改其他平台、不改 `interface`/`core`/`stream`/`app`。

---

### Task 1: 更新 RV1126B 视频流水线常量适配 IMX415

**Files:**
- Modify: `modules/platform/rv1126b/src/rv_video_pipeline.cpp:23-33`

**Interfaces:**
- Consumes: 无（常量为匿名命名空间内部定义）
- Produces: 无新接口；`set_vi_channel` / `create_venc_channel` / `bind_vi_to_venc` 等函数继续消费这些常量，签名不变。

**依据（来自 spec §5.2）：** IMX415 mode[0] = 3864×2192@30fps 10bit Linear；主码流用 sensor native（ISP 无缩放），子码流 720p，MJPEG 随 VI Chn2 广播变 720p。

- [ ] **Step 1: 编辑常量块**

对 `modules/platform/rv1126b/src/rv_video_pipeline.cpp`，把行 23-33：

```cpp
constexpr int kVencMjpegChn = 2;
constexpr int kMainWidth = 2688;
constexpr int kMainHeight = 1520;
constexpr int kSubWidth = 640;
constexpr int kSubHeight = 480;
constexpr int kMjpegWidth = 640;
constexpr int kMjpegHeight = 480;
constexpr int kFrameRate = 30;
constexpr int kMainBitrate = 4096;
constexpr int kSubBitrate = 1024;
constexpr int kMjpegBitrate = 2048;
```

替换为：

```cpp
constexpr int kVencMjpegChn = 2;
// 视频参数对应 IMX415 mode[0]：sensor native 3864x2192@30fps 10bit Linear（NO_HDR），
// 由 rockit 默认协商，无需应用层显式设置（与官方 test_mpi_vi.cpp 流程一致）。
// 主码流 stSize = sensor native，ISP 无缩放；子码流/MJPEG 由 ISP 下采样。
// 详见 atk_dlrv1126b_linux6.1_sdk/kernel-6.1/drivers/media/i2c/imx415.c supported_modes[0]。
constexpr int kMainWidth = 3864;
constexpr int kMainHeight = 2192;
constexpr int kSubWidth = 1280;
constexpr int kSubHeight = 720;
constexpr int kMjpegWidth = 1280;
constexpr int kMjpegHeight = 720;
constexpr int kFrameRate = 30;
constexpr int kMainBitrate = 8192;
constexpr int kSubBitrate = 2048;
constexpr int kMjpegBitrate = 2048;
```

改动说明：分辨率 `kMain 2688×1520→3864×2192`、`kSub/kMjpeg 640×480→1280×720`；码率 `kMainBitrate 4096→8192`、`kSubBitrate 1024→2048`；`kMjpegBitrate`/`kFrameRate`/通道号不变。VI/VENC 函数体不动。

- [ ] **Step 2: 交叉编译验证**

Run: `TARGET_PLATFORM=rv1126b make`
Expected: 编译成功，无新警告。这些改动仅改 `constexpr int` 数值，类型与下游 `VI_CHN_ATTR_S.stSize`/`VENC_CHN_ATTR_S` 填充逻辑不变，编译行为不变。

若工具链不在 PATH 导致 cmake 配置失败（报找不到 `aarch64-buildroot-linux-gnu-gcc`），先确认工具链 bin 目录已加入 PATH，再重试。这是环境前置，非代码问题。

- [ ] **Step 3: 提交**

```bash
git add modules/platform/rv1126b/src/rv_video_pipeline.cpp
git commit -m "feat(rv1126b): 视频流水线参数适配 IMX415（主码流 8MP、子码流 720p）"
```

- [ ] **Step 4: 设备侧功能验证（用户在目标设备执行）**

部署 `eye` 到设备 `/app/bin/eye` 后：

1. 运行 `rk_dumpsys`（即 `scripts/rk_dumpsys_compat.sh`），核对：
   - sensor 实际输出 ≈ 3864×2192@30
   - VI Chn3 输出 3864×2192、VI Chn2 输出 1280×720
   - VENC CHN0 = 3864×2192 H265@30、CHN1 = 1280×720 H264@30、CHN2 = 1280×720 MJPEG@10
2. RTSP 拉流：
   - `ffprobe rtsp://<设备IP>:8554/main` → 期望 3864×2192，H265
   - `ffprobe rtsp://<设备IP>:8554/sub` → 期望 1280×720，H264
3. `perf top -p $(pidof eye)` 观察编码器负载（验证 8MP@30 编码能力，见 spec §7）。
4. 持续运行 5~10 分钟，检查日志有无码流 buffer 不足/丢帧告警。

预期：主/子码流分辨率与帧率符合上表。若主码流出现编码失败/严重丢帧/花屏，转入 Task 2 降级。

---

### Task 2（条件执行）: 若 8MP 编码不支撑则降级 4MP

**仅在 Task 1 Step 4 设备验证发现 8MP@30 H265 编码失败/丢帧/花屏时执行。** 若 8MP 验证通过则跳过本任务。

**Files:**
- Modify: `modules/platform/rv1126b/src/rv_video_pipeline.cpp:24-25,31`

**依据（spec §7 高风险项缓解）：** RV1126B 硬件 VENC 的 8MP@30 编码能力未预先核实。降级到标准 4MP（2560×1440）是已知可工作的档位（海思 SC4336P 同档），ISP 从 sensor native 下采样。

- [ ] **Step 1: 降级主码流常量**

把 Task 1 中三行改回较低档：

```cpp
constexpr int kMainWidth = 2560;       // 原 3864，降级 4MP
constexpr int kMainHeight = 1440;      // 原 2192
...
constexpr int kMainBitrate = 4096;     // 原 8192，4MP 档位
```

（`kSubWidth/Height`、`kMjpegWidth/Height`、`kSubBitrate` 保持 Task 1 的 720p/2048 不变。）

- [ ] **Step 2: 重新交叉编译**

Run: `TARGET_PLATFORM=rv1126b make`
Expected: 编译成功。

- [ ] **Step 3: 提交**

```bash
git add modules/platform/rv1126b/src/rv_video_pipeline.cpp
git commit -m "fix(rv1126b): 主码流降级 4MP（RV1126B 8MP 编码能力不足）"
```

- [ ] **Step 4: 设备复验**

部署后用 `ffprobe rtsp://<设备IP>:8554/main` 确认 2560×1440 H265@30 稳定无丢帧。

---

## Self-Review

**1. Spec coverage:**
- spec §5.2 常量重定义 → Task 1 Step 1 ✓
- spec §5.3 sensor mode 跟随官方示例（不改代码）→ Global Constraints + Task 1 依据说明 ✓（无代码动作，正确）
- spec §5.4/5.5 VI/VENC 随常量生效 → Task 1 说明"函数体不动" ✓
- spec §7 高风险（8MP 编码）→ Task 2 条件降级 ✓
- spec §9 验证步骤 → Task 1 Step 4 ✓
- spec §6 隔离性 → Global Constraints ✓

**2. Placeholder scan:** 无 TBD/TODO；所有 step 含完整代码或确切命令。

**3. Type consistency:** 仅改 `constexpr int` 数值，无新类型/函数，无跨 task 类型一致性问题。

**结论：** plan 完整覆盖 spec，无占位符，无类型不一致。可执行。
