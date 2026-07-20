# RV1126B dumpsys 输出空白问题

## 现象

- `eye` 进程启动正常，rockit 内核模块加载正常，RTSP 视频流可正常播放。
- `/usr/bin/dumpsys venc` 输出仅 208 字节（只有头/尾，无通道数据）。
- `/usr/bin/dumpsys sys` 绑定表为空。
- `/usr/bin/dumpsys vi` 通道属性正常显示，但所有动态计数器（`frame_id`、`framerate`、`get_cnt`、`release_cnt`）全为 0。

## 根因（多源交叉验证）

SDK 内部两个 rockit 源不同步：

| 组件 | SDK 路径 | 版本 | 构建时间 |
|---|---|---|---|
| 内核模块 rockit-ko | `external/ipc_drv_ko/rockit/release_rockit-ko_rv1126b_arm64_asm/` | `vmpi:9ca741946f...-v2.48.0` | 2025-12-30 |
| 用户态 librockit.so + dumpsys | `external/rockit/lib/arm64/rv1126b/linux/` + `external/rockit/mpi/example/bin/arm64/rv1126b/dumpsys` | `git-5057bd373` | 2025-12-15 |

两者相差 15 天，DumpSys 相关 ioctl 协议在此期间演进，导致不兼容。

## 证据链

1. `strace dumpsys venc`：`connect(127.0.0.1:3893)=0` → `sendto("/rockit/mpi/dump#{\"module\":\"venc\"}#")` → `recvfrom()=0` 字节。
2. `netstat -tlnp | grep 3893`：`127.0.0.1:3893 LISTEN 803/eye`（eye 内置 `ipcs_server` 线程处理 dumpsys 请求）。
3. `readelf -d /usr/bin/dumpsys`：NEEDED 仅 `libstdc++.so.6 / libm.so.6 / libgcc_s.so.1 / libc.so.6`，**未链接 librockit.so**；`nm` 中**无 `RK_MPI_SYS_DumpSys` 符号**。
4. `rk_mpi_sys_test` 运行报告：`failed to RK_MPI_SYS_GetBindbySrc` 和 `failed to RK_MPI_SYS_GetBindbyDest` — 直接证明 librockit↔内核协议错配。
5. `cat /dev/mpi/vsys` 直接读取：内核侧节点 33/38（rkvpss-vir0）→ 节点 44/47/50（venc）绑定关系完整，帧计数随时间增长（30 fps × 5 节点），证明内核侧视频流确实在跑。
6. 设备 `/usr/lib/librockit.so` 与 SDK 中所有 librockit.so 一致（`git-5057bd373`，Dec 15）；设备 `/usr/lib/module/rockit*.ko` 与 SDK `buildroot/output/.../target/usr/lib/module/` 完全一致（md5 匹配，v2.48.0，Dec 30）。

## 架构说明

```
+-----------+   TCP 127.0.0.1:3893    +----------+   ioctl    +-----------+
|  dumpsys  | ---------------------> |   eye    | ---------> | rockit.ko |
| (client)  |  /rockit/mpi/dump#...  | (server) |            |  (kernel) |
+-----------+                        +----------+            +-----------+
     |                                    |                       ^
     | 不链接 librockit.so                 | librockit.so          | /dev/mpi/*
     | (无 RK_MPI_SYS_DumpSys 符号)        | (Dec 15)              |
     |                                    v                       |
     |                              协议错配，ioctl 失败            |
     |                                    |                       |
     v                                    v                       v
  recvfrom()=0                       返回空 buf              实际状态正常
  打印头/尾，退出码 1
```

eye 之所以能正常工作（VI/VENC/Bind/GetStream）：这部分 ioctl 接口在两版本间保持稳定；只有 DumpSys 系列查询接口演进导致失败。

## 临时 workaround：`rk_dumpsys_compat.sh`

绕过 librockit，直接 `cat /dev/mpi/*` 内核接口。

### 部署

```bash
# 从 eye 仓库 scripts/ 复制到设备
scp scripts/rk_dumpsys_compat.sh root@<device>:/usr/bin/rk_dumpsys
ssh root@<device> 'chmod +x /usr/bin/rk_dumpsys'
```

### 用法

```bash
rk_dumpsys                  # 全量快照
rk_dumpsys bind             # 绑定关系 + 节点帧计数（最常用）
rk_dumpsys sys              # 版本 + 绑定 + dev 列表
rk_dumpsys mb               # MB 内存分配
rk_dumpsys venc             # VENC buf 列表
rk_dumpsys watch 2          # 持续 fps 采样（2 秒间隔）
rk_dumpsys version          # 仅版本信息
rk_dumpsys eye 30           # eye 日志最近 30 行
rk_dumpsys log 30           # 内核 rockit 日志最近 30 行
```

### 局限

- 仅能读取 vsys/valloc/venc/vlog 节点暴露的字段；其他节点（vvi/aiisp/avs/gdc/ivs）的 proc-style read 返回空。
- 无法获取 dumpsys 各模块的细粒度属性（如 VI chn attr 的 `width/height/buf_count`、VENC 的 `rc_mode/gop` 等），需通过 eye 日志或源码确认。
- 帧计数含义：
  - `rkvpss-vir0` 节点用 `onfa_cnt`（输出到下游的帧数）
  - `venc` 节点用 `infa_cnt`（从上游接收的帧数）

### 字段映射

`/dev/mpi/vsys` 节点列表字段顺序：

```
1=id 2=name 3=handle 4=nid(=VI/VENC 通道号) 5=uid 6=ref
7=infa_cnt 9=in_frate 14=onfa_cnt 16=out_frate 23=state 24..=next_node
```

`nid` 字段对应 eye 的通道号：
- `rkvpss-vir0 nid=3` ↔ eye `kViMainChn = 3`（主码流 VI 通道）
- `rkvpss-vir0 nid=2` ↔ eye `kViSubChn = 2`（子码流 VI 通道）
- `venc nid=0/1/2` ↔ eye `kVencMainChn/SubChn/MjpegChn`

## 长期根修复路径

需要 Rockchip/Alientek 提供匹配的 librockit.so + dumpsys 二进制（与 `external/ipc_drv_ko` 同源，commit `9ca741946f7730ce7e7693381ba90ab131d633e5`，v2.48.0）。

获取后：

1. 替换 SDK：
   - `external/rockit/lib/arm64/rv1126b/linux/librockit.so`
   - `external/rockit/mpi/example/bin/arm64/rv1126b/dumpsys`
2. 重新编译 rootfs：`cd buildroot && make rockit-rebuild && make`（或全量重建）。
3. 重新部署到设备后，验证：
   - `dumpsys version` 应显示与内核一致的版本（`vmpi:9ca741946f...-v2.48.0`）
   - `dumpsys venc` 应显示 3 个 VENC 通道的完整属性表
   - `dumpsys sys` 应显示 `rkvpss-vir0 -> venc` 的绑定关系表，`src_recv_cnt` / `dst_recv_cnt` 非零
4. 验证通过后，可移除 `rk_dumpsys_compat.sh` 临时脚本。

## 临时 workaround 不应触碰的项

- 不要替换 `/usr/lib/librockit.so`：当前 eye 运行依赖它的稳定 ioctl 接口；不匹配版本可能引入新问题。
- 不要降级 `/usr/lib/module/rockit*.ko`：会破坏 eye 当前正常工作的视频流。
- 不要重启 eye 或 ko：会丢失当前运行时状态，且不能修复版本错配。
