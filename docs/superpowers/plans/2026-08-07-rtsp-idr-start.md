# RTSP IDR 起播实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让每次 RTSP PLAY 和重新 PLAY 都从解析器确认的真实 IDR NAL 开始交付，消除连接初始的 HEVC 参考帧错误。

**Architecture:** 在每个 `RtspStreamSource` 内维护独立的等待 IDR 状态。source 从共享队列取 NAL 时丢弃 IDR 之前的数据，找到 IDR 后沿现有 live555 交付路径发送；停止取帧时恢复等待状态。

**Tech Stack:** C++11、live555、CMake、RV1126B aarch64 交叉编译工具链、ffmpeg。

## Global Constraints

- 新 RTSP PLAY 和重新 PLAY 必须从解析器确认的真实 IDR NAL 开始交付。
- H.264 和 H.265 使用相同的 source 状态机。
- 不引入 RTSP 到平台 VENC 的反向依赖。
- 不缓存 8MP 关键帧，不改变编码参数、GOP、码率或分辨率。
- 保留 1 MiB `OutPacketBuffer` 和 `fDurationInMicroseconds = 0` 修复。
- 所有构建均使用 RV1126B 交叉编译工具链，产物不能在 PC 上直接运行。
- 未经用户单独授权不得写设备或重启；未经用户明确要求不得创建 Git commit。

---

## File Structure

- `test/rtsp/rtsp_unit_tests.cpp`：增加基于真实 live555 `FramedSource::getNextFrame()` 的起播回归测试。
- `modules/rtsp/include/rtsp_stream_source.h`：保存每个 RTSP source 的等待 IDR 状态。
- `modules/rtsp/src/rtsp_stream_source.cpp`：实现首次 PLAY 和重新 PLAY 的 IDR 门控。

### Task 1: 实现每个 Source 的 IDR 起播门控

**Files:**
- Modify: `test/rtsp/rtsp_unit_tests.cpp`
- Modify: `modules/rtsp/include/rtsp_stream_source.h:31-35`
- Modify: `modules/rtsp/src/rtsp_stream_source.cpp:12-84`

**Interfaces:**
- Consumes: `RtspNalUnit::is_idr`，由 `rtsp_nal_parser.cpp` 根据 H.264/H.265 NAL header 填充。
- Produces: `RtspStreamSource::waiting_for_idr_`，仅在 source 内部使用，不新增公共 API。

- [ ] **Step 1: 写首次 PLAY 的失败测试**

在 `test/rtsp/rtsp_unit_tests.cpp` 增加 live555 环境、回调结果和测试：

```cpp
#include "rtsp_stream_source.h"

#include <BasicUsageEnvironment.hh>

struct DeliveryResult {
    bool called;
    unsigned frame_size;
};

static void after_getting_frame(void* client_data, unsigned frame_size,
                                unsigned, timeval, unsigned) {
    DeliveryResult* result = static_cast<DeliveryResult*>(client_data);
    result->called = true;
    result->frame_size = frame_size;
}

static void test_stream_source_waits_for_idr_on_first_play() {
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);
    std::shared_ptr<RtspFrameQueue> queue(new RtspFrameQueue(4));

    RtspNalUnit p_nal = make_nal(0x01, 100);
    p_nal.is_idr = false;
    queue->push_nal_unit(std::move(p_nal));
    RtspNalUnit idr_nal = make_nal(0x26, 200);
    idr_nal.is_idr = true;
    queue->push_nal_unit(std::move(idr_nal));

    RtspStreamSource* source = RtspStreamSource::createNew(
        *env, StreamType::VIDEO_MAIN, CodecType::H265, queue);
    uint8_t output[16] = {};
    DeliveryResult result = {false, 0};
    source->getNextFrame(output, sizeof(output), after_getting_frame, &result,
                         NULL, NULL);

    assert(result.called);
    assert(result.frame_size == 1);
    assert(output[0] == 0x26);

    Medium::close(source);
    env->reclaim();
    delete scheduler;
}
```

在 `main()` 中调用 `test_stream_source_waits_for_idr_on_first_play()`。

- [ ] **Step 2: 交叉编译测试并确认当前行为不满足断言**

运行：

```bash
TARGET_PLATFORM=rv1126b make debug
```

预期：ARM64 测试二进制成功生成。若用户批准将测试程序写入设备，则运行 Debug 测试并确认 `output[0] == 0x26` 断言失败、实际为 `0x01`；若用户不批准设备写入，明确记录只能完成编译验证。

- [ ] **Step 3: 实现首次 PLAY 的最小 IDR 门控**

在 `modules/rtsp/include/rtsp_stream_source.h` 增加私有状态：

```cpp
bool awaiting_frame_;
bool waiting_for_idr_;
```

构造函数初始化：

```cpp
codec_type_(codec),
awaiting_frame_(false),
waiting_for_idr_(true) {
```

将 `deliver_frame()` 的单次弹出改为等待 IDR 的循环：

```cpp
RtspNalUnit nal;
for (;;) {
    if (!source->frame_queue_->pop_nal_unit(nal)) {
        source->awaiting_frame_ = true;
        return;
    }
    if (!source->waiting_for_idr_ || nal.is_idr) {
        source->waiting_for_idr_ = false;
        break;
    }
}
```

保持后续大小检查、presentation time、`fDurationInMicroseconds = 0` 和 `FramedSource::afterGetting()` 不变。

- [ ] **Step 4: 增加重新 PLAY 的失败测试**

在首次交付 IDR 后执行 `source->stopGettingFrames()`，再向队列依次压入 P NAL 和 IDR NAL，重新调用 `getNextFrame()`；断言第二次仍交付 `0x26` 而不是 `0x01`。

```cpp
source->stopGettingFrames();

RtspNalUnit resumed_p = make_nal(0x01, 300);
resumed_p.is_idr = false;
queue->push_nal_unit(std::move(resumed_p));
RtspNalUnit resumed_idr = make_nal(0x26, 400);
resumed_idr.is_idr = true;
queue->push_nal_unit(std::move(resumed_idr));

output[0] = 0;
result = {false, 0};
source->getNextFrame(output, sizeof(output), after_getting_frame, &result,
                     NULL, NULL);
assert(result.called);
assert(output[0] == 0x26);
```

- [ ] **Step 5: 实现重新 PLAY 状态重置**

修改 `doStopGettingFrames()`：

```cpp
void RtspStreamSource::doStopGettingFrames() {
    awaiting_frame_ = false;
    waiting_for_idr_ = true;
    FramedSource::doStopGettingFrames();
}
```

- [ ] **Step 6: 重新交叉编译并检查差异**

运行：

```bash
TARGET_PLATFORM=rv1126b make debug
TARGET_PLATFORM=rv1126b make
git diff --check
```

预期：Debug/Release 的 `eye` 和 `rtsp_unit_tests` 均成功链接，`git diff --check` 无输出。

### Task 2: 设备部署与零错误起播验证

**Files:**
- Deploy: `build/rv1126b-release/app/eye` -> `/app/bin/eye`
- Preserve: `/app/bin/eye.bak`

**Interfaces:**
- Consumes: Task 1 生成的 RV1126B Release `eye`。
- Produces: 三次独立连接均零解码错误的设备验证证据。

- [ ] **Step 1: 核对产物和设备空间并请求写入批准**

运行本地产物 `stat`、`sha256sum`，并通过 SSH 只读检查 `/app` 可用空间和当前程序哈希。向用户披露目标路径、大小、覆盖和重启风险，再取得明确批准。

- [ ] **Step 2: 上传、校验并原子替换**

SCP 到 `/app/bin/eye.new`，比较本地与设备 SHA-256；一致后设置 `0755`，移动为 `/app/bin/eye`，执行 `sync`。保留 `/app/bin/eye.bak` 不变。

- [ ] **Step 3: 重启并等待服务恢复**

执行 `reboot`，轮询 SSH、`/app/bin/eye` 进程和 TCP 8554 监听状态，超时 180 秒。

- [ ] **Step 4: 连续验证三次主流起播**

每次建立全新的 TCP RTSP 连接：

```bash
ffmpeg -hide_banner -loglevel error -rtsp_transport tcp \
  -i 'rtsp://172.20.8.114:8554/main' -t 15 -an -f null -
```

预期：三次命令均退出码 0 且无 HEVC 解码错误输出。每次连接允许等待下一个 IDR，最长约 2 秒。

- [ ] **Step 5: 验证子流和设备日志**

运行 15 秒 `/sub` 解码，预期无 H.264 错误。只读检查 `/tmp/eye_warn.log` 和 `/tmp/eye_info.log`，预期不存在 `NAL truncated`、`Frame queue overflow`、VENC Get/Release 错误。

- [ ] **Step 6: 记录最终运行状态**

记录 `/app/bin/eye` SHA-256、进程 PID、8554 监听、主流 `3864x2192@30fps`、子流 `1280x720@30fps`，并报告原始备份 `/app/bin/eye.bak` 仍可用于恢复。
