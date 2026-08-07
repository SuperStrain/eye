# RTSP IDR 起播设计

## 背景

RV1126B 主码流为 H.265 3864x2192@30fps。修复 live555 大 NAL 截断和 source 固定 duration 节流后，持续花屏、RTSP 队列溢出和周期性参考帧缺失已经消失，但客户端从 GOP 中间连接时仍可能在首个随机访问帧到达前报告一次参考帧缺失。

## 目标

- 新 RTSP PLAY 和重新 PLAY 必须从解析器确认的真实 IDR NAL 开始交付。
- H.264 和 H.265 使用相同的 source 状态机。
- 不引入 RTSP 到平台 VENC 的反向依赖。
- 不缓存 8MP 关键帧，不改变编码参数、GOP、码率或分辨率。

## 设计

`RtspStreamSource` 增加布尔状态 `waiting_for_idr_`：

- 构造时设置为 `true`。
- `deliver_frame()` 在等待状态下持续从 `RtspFrameQueue` 弹出 NAL。
- `nal.is_idr` 为 false 时丢弃该 NAL 并继续检查队列。
- 找到首个 `nal.is_idr` 后将状态改为 `false`，并按现有路径交付该 IDR。
- 队列为空时设置 `awaiting_frame_` 并返回，等待下一次 frame event，不进行忙轮询。
- `doStopGettingFrames()` 将 `waiting_for_idr_` 恢复为 `true`，保证重新 PLAY 不复用旧参考链。

IDR 判断使用 `rtsp_nal_parser.cpp` 对 Annex-B NAL header 的解析结果。H.264 仅 NAL type 5 视为 IDR；H.265 仅 NAL type 19/20 视为 IDR。不得使用 Rockit `VENC_PACK_S::DataType` 中无法区分随机访问类型的普通 I-slice 标记代替。

## 边界条件

- 参数集继续通过现有 `ParameterSetCache` 写入 SDP；source 等待阶段不向 RTP sink 发送 VPS/SPS/PPS。
- 最长起播等待由当前 60 帧 GOP 决定，约为 2 秒。
- 等待期间若 RTSP 队列溢出，仍按完整 access unit 丢弃最旧数据；后续真实 IDR 到达时可以恢复。
- 大 NAL 继续使用 1 MiB `OutPacketBuffer`，每个 NAL 的 duration 保持为 0，避免 source 端重复限速。

## 验证

- 增加等待 IDR 状态转换的回归测试。
- RV1126B Debug 和 Release 交叉编译成功。
- 部署后连续建立三次 TCP RTSP 连接，每次解码 15 秒，ffmpeg 不报告 HEVC/H.264 解码错误。
- 设备日志不出现 `NAL truncated` 或 `Frame queue overflow`。
- `/main` 保持 H.265 3864x2192@30fps，`/sub` 保持 H.264 1280x720@30fps。
