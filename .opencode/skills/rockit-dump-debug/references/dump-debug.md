# Rockit MPI DUMP 调试参考

## 来源与提取

- 来源：`/home/yangy/atk_dlrv1126b_linux6.1_sdk/external/rockit/mpi/doc/Rockchip_Developer_Guide_MPI_CN.pdf`
- 范围：PDF 第 610-663 页，章节标题为“Dump调试信息说明”；PDF 元数据生成日期：2024-10-16。
- 提取方式：先用 `pdfgrep -n -i "Dump调试信息说明|DUMP调试信息说明"` 核对目录页和正文起点，再用 `pdftotext -f ... -l ... -layout` 分段提取，并逐页提取第 610-663 页核对标题、命令、表头和参数说明。
- 本文件只转录上述页中的命令、字段和解释。PDF 表格存在换行、拼写或字段名不一致时保留原文含义，并明确标注“PDF 原文如此”，不推断文档外参数。

## 全局使用前提

- 基本格式：`dumpsys 模块名`。
- 多模块格式：`dumpsys mod1-mod2-mod3-...`，例如 `dumpsys vdec-vpss-vo`。
- 应用必须已经调用 `RK_MPI_SYS_Init`。
- 应用进程必须保持运行；进程退出后无法取得该应用的 DUMP 结果。
- 命令不存在、模块未注册或字段与本文不同，应按 SDK/固件版本差异处理，不得编造替代节点。
- `dumpsys 模块名` 及多模块 DUMP 为读取当前配置和状态的命令；下文另行标注的 `record`、`viraddr`、`open_sync_debug`、`close_sync_debug` 会写文件或改变调试状态，均为**非只读，需单独批准**。

## SYS

**用途**：记录当前 SYS 模块使用情况；通过绑定关系、收发计数和速率判断模块间数据是否流动。

**命令**：`./dumpsys sys`

**关键表/字段**：`src_mod/src_dev/src_chn` 是发送侧模块、设备号、通道号；`dst_mod/dst_dev/dst_chn` 是接收侧对应字段；`src_recv_cnt` 是第一级接收数据计数，`dst_recv_cnt` 是第一级向第二级发送数据计数；`src_recv_rate`、`dst_recv_rate` 是对应数据帧率（PDF 说明一般以帧为单位）。

**异常模式**：绑定关系、发送侧/接收侧计数和帧率可作为数据链路的辅助判断线索；计数或帧率不增长/不一致时，需结合相邻模块 DUMP 确认，不能仅据此确定根因。

**注意事项**：表头为 `bind relation table`；不要把 `src_recv_cnt` 和 `dst_recv_cnt` 的方向互换。

**来源页**：611-612。

## MB

**用途**：记录 MB buffer 使用情况，查看总量、模块/通道占用及具体 block。

**命令**：

```text
dumpsys mb
dumpsys mb d
```

**关键表/字段**：`mb total statistics` 包含 `mb_type`、`total_size`、`total_cnt`；`mb module info` 包含 `mod`、`chn_cnt`、`total_size`、`total_cnt`；`mb channel info` 包含 `owner`、`mod`、`dev_id`、`chn_id`、`total_size`、`total_cnt`、`unused_cnt`；`mb detail info` 包含 `owner`、`mod`、`dev_id`、`chn_id`、`blk`、`virt`、`fd`、`phy_addr`、`size`、`length`、`refs`。

**异常模式**：总量、模块/通道占用、空闲数以及 detail 中的 `refs`、`unused_cnt` 可作为 buffer 占用、引用或长度问题的辅助线索；PDF 未给出固定阈值，需结合其他模块确认。

**注意事项**：`dumpsys mb d` 才显示 detail，可从 `virt` 获取虚地址。以下命令把该虚拟地址对应物理内存数据写入文件，**非只读，需单独批准**：

```text
./dumpsys mb viraddr 0x7f8b41c000 /tmp/mb.bin
```

该例中的地址和文件名是 PDF 示例，不得当作固定地址或固定输出路径。

**来源页**：613-615。

## VPSS

**用途**：记录 VPSS 属性配置和状态，涵盖组、通道、裁剪、队列、当前图像及通道工作状态。

**命令**：`dumpsys vpss`

**关键表/字段**：`vpss mod param.mb_source` 表示 COMMON、PRIVATE 或 USER 内存缓冲来源；group attr 包含 `grp_id/max_w/max_h/pixel_format/dym_range/src_rate/dst_rate/is_compress/rotate/vproc_dev`；channel attr1 包含 `grp_id/chn_id/mode/width/height/pixel_format/is_compress/src_rate/dst_rate/depth/align/mirror/flip/frm_cnt`；channel attr2 包含 `rotate/aspect/video_x/video_y/video_w/video_h/bg_color`；group/chn crop 包含 `crop_en/coor_type/x/y/width/height`；group pic queue 包含 `delay/backup/lft_in_cnt/lft_out_cnt/now_cst_us/avg_cst_us/max_cost_us/run_cnt`；group pic info 包含 `width/height/vir_w/vir_h/pix_format/dyn_range/compress`；channel work status 包含 `get_frm_cnt/get_frm_rate/rel_frm_cnt/lft_in_cnt/lft_out_cnt/now_cst_us/avg_cst_us/max_cst_us/run_cnt`；AI ISP 相关表包含 `enable/callback/data/module_path/frm_cnt` 及 `lft_in_cnt/lft_out_cnt/now_cst_us/avg_cst_us/max_cst_us/run_cnt/now_itv_us/avg_itv_us/max_itv_us`。

**异常模式**：队列 `lft_in_cnt/lft_out_cnt`、处理耗时、计数和帧率可作为输入积压、输出未取走、处理耗时或通道状态的辅助线索；`get_frm_cnt` 与 `rel_frm_cnt` 的差异需结合取帧/还帧流程确认。

**注意事项**：旋转信息显示的前提是成功调用 `RK_MPI_VPSS_SetChnRotation` 或 `RK_MPI_VPSS_SetChnRotationEx`，并配置非 0 角度。相对坐标/绝对坐标、压缩 Y/N 和 USER/AUTO 等枚举以 PDF 表格为准。

**来源页**：615-619。

## RGN

**用途**：记录区域资源以及 OVERLAY、COVER、MOSAIC 在通道中的显示状态。

**命令**：`./dumpsys rgn`

**关键表/字段**：OVERLAY 资源表包含 `hdl/type/used/pixel_format/width/height/mb/virt/clut_num`；OVERLAY 通道表包含 `mod/dev/chn/is_show/x/y/fg_alpha/bg_alpha/layer`；COVER 资源表包含 `hdl/type/used`，通道表另含 `width/height/color/coord_type`；MOSAIC 资源表包含 `hdl/type/used`，通道表包含 `mod/dev/chn/is_show/x/y/width/height/point_0..point_3/blk_size/layer`。

**异常模式**：`used`、`is_show`、挂接模块/设备/通道和坐标可作为区域占用、显示状态及位置的辅助判断线索，需结合其他模块确认。

**注意事项**：类型值为 OVERLAY=0、COVER=1、MOSAIC=2；PDF 对 `piont_0` 等字段存在拼写，按表头实际输出识别。`coord_type` 为 RATIO 或 ABS。

**来源页**：619-622。

## VGS

**用途**：记录 VGS 最近完成任务、最近耗时最大的任务以及累计 job/task 状态。

**命令**：`dumpsys vgs`

**关键表/字段**：模块参数为 `g_max_job_num/g_max_task_num`；job info 为 `seq_no/job_hdl/state/task_num/in_size/out_size/cost_time/hw_time`，并有 `scale/cover/mosaic/osd/line/rotate/crop`；task 图像信息包含 `srcw/srch/src_vir_w/src_vir_h/srcformat/src_rect_x/src_rect_y/src_rect_w/src_rect_h` 及对应 `dst*`；job status 为 `success/fail/cancel/all_job_num/free_num/begin_num/busy_num/procing_num`；task status 为 `success/fail/cancel/all_task_num/free_num/busy_num`。

**异常模式**：累计 `fail/cancel`、资源 `free_num`、正在处理数和 `cost_time/hw_time` 可作为任务失败、取消、资源使用或耗时异常的辅助线索，需结合其他模块确认。

**注意事项**：以下录制会产生输入/输出数据文件，**非只读，需单独批准**：

```text
./dumpsys vgs record /tmp/ 10
```

PDF 说明最多录制 10 帧。

**来源页**：623-625。

## ADEC

**用途**：记录音频解码属性配置和状态。

**命令**：`dumpsys adec`

**关键表/字段**：通道属性/状态包含 `chn_id/codec_id/buf_cnt/mode/rate/channel/orig_send_cnt/send_cnt/get_cnt/put_cnt`；其中 send 是成功发送解码器的音频帧数，get/put 是用户获取/释放音频帧数。

**异常模式**：`orig_send_cnt`、`send_cnt`、`get_cnt`、`put_cnt` 的差异可作为发送、解码或取放帧不匹配的辅助线索，需结合其他模块确认。

**注意事项**：`mode` 区分按流解码或按帧解码；不要从 PDF 外推 codec 枚举。

**来源页**：625-626。

## AENC

**用途**：记录音频编码属性配置和状态。

**命令**：`dumpsys aenc`

**关键表/字段**：属性包含 `chn_id/codec_id/buf_cnt/rate/channel/bit_with`；状态包含 `chn_id/recv_frame/enc_ok/frame_err/get_stream/release_stream`。

**异常模式**：`frame_err`、`get_stream` 与 `release_stream` 的数值关系可作为音频编码失败或码流取放问题的辅助线索，需结合其他模块确认。

**注意事项**：字段名 `bit_with` 是 PDF 表头原文。

**来源页**：626-627。

## AO

**用途**：记录音频输出属性、设备扩展信息、通道和节点状态。

**命令**：`dumpsys ao`

**关键表/字段**：设备属性包含 `ao_dev/snd_rate/snd_channel/snd_bit_Width/data_rate/data_channel/data_bit_width/chn_cnt/expand_flag/frm_num/frm_size`；扩展状态为 `track_mode/mute/volume`；通道属性为 `ao_chn/card_name/snd_open/state/resample_open/in_rate/in_ch/out_rate/out_ch`；通道状态为 `frm_len/frm_total_cnt/frm_total_len`；节点状态为 `node/in_size/in_cnt/proc_cnt/write_size`。

**异常模式**：`snd_open/state/resample_open`、累计送帧计数/长度和节点 `in_cnt/proc_cnt/write_size` 可作为设备状态、重采样和播放回调推进情况的辅助线索，需结合其他模块确认。

**注意事项**：PDF 对 `track_mode`、静音和音量给出枚举/范围；`proc_cnt`、`write_size` 只统计 playback 节点。

**来源页**：627-629。

## AI

**用途**：记录音频输入属性、设备扩展信息、通道和节点状态。

**命令**：`dumpsys ai`

**关键表/字段**：设备属性包含 `ai_dev/snd_rate/snd_channel/snd_bit_Width/data_rate/data_channel/data_bit_width/chn_cnt/expand_flag/frm_num/frm_size`；扩展信息为 `track_mode`；通道属性为 `ai_chn/card_name/snd_open/state/resample_open/in_rate/in_ch/out_rate/out_ch`；通道状态为 `frm_len/frm_total_cnt/frm_total_len`；节点状态为 `node/in_size/in_cnt`。

**异常模式**：设备打开状态、通道状态、重采样参数、累计取帧计数和节点输入计数可作为音频输入状态或积压的辅助线索，需结合其他模块确认。

**注意事项**：PDF 对 `chn_cnt/expand_flag/frm_num` 标注暂未使用及默认值；不应据此补充其他字段。

**来源页**：629-631。

## VI

**用途**：记录 VI 模块参数、设备/管道/通道配置、查询状态、buffer、用户图片、连接、EDID、codec 和 DIS 信息。

**命令**：`dumpsys vi`

**关键表/字段**：模块参数为 `vi_max_chn_num`；通道属性为 `pipe/chn/width/height/max_width/max_height/compress_mode/memory_type/buf_type/pix_format/buf_count/buf_size/entity_name`；查询状态为 `enabled/input_lost/output_lost/frame_id/framerate/vbfail/freeze`；buffer 状态为 `seq/pts/delay_us/poll_cnt/poll_fail_cnt/get_cnt/release_cnt/qbuf_fail_cnt/commit_cnt/refs`；用户图片为 `enable/mode/bg_color/frame/width/height/pix_format/refs/seq/pts`；连接信息为 `pix_format/connect`；EDID 为 `max_block/block_id/pad/edid_crc`；stream 为 `codec_type`；DIS 配置为 `mode/motion/pdt_type/buf_num/crop_ratio/frm_rate/gyro_range/gyro_bit_width/cam_steady`；DIS 属性表头使用 `timelag`，参数说明使用 `time_lag`，这是 PDF 的表头/参数名差异，表示同一时间差字段，不是两个字段；其余属性为 `dis_chn/enable/mov_sub/roef/view_angle/still_crop/hor_limit/ver_limit`。

**异常模式**：`input_lost/output_lost/vbfail` 可作为输入、输出丢帧和取帧失败的辅助线索；`poll_fail_cnt` 持续增加可作为查询 buffer 失败的线索，PDF 提示可能与驱动异常有关，仍需结合其他模块确认；`get_cnt/release_cnt`、`qbuf_fail_cnt`、`refs` 可辅助检查 buffer 获取、归还和引用；`connect` 可为 unknown/connect/disconnect。

**注意事项**：`compress_mode`、`memory_type`、`buf_type` 枚举按 PDF 表格解释；`max_width/max_height` 标注暂时不用；DIS 的 `view_angle` 标注无效、未使用；PDF 参数说明使用 `time_lag`，而 DUMP 表头使用 `timelag`，应保留两种写法以对应原文。不要用文档外节点替代本命令。

**来源页**：631-635。

## VO

**用途**：记录 VO 设备、图层绑定、图层/通道和回写状态。

**命令**：`dumpsys vo`

**关键表/字段**：设备为 `DevId/DevEn/InfType/InfSync/Vcnt`；图层绑定为 `DevId/Video/Gfx/Cursor`；图层状态为 `LayId/LayEn/PixFmt/ImgW/ImgH/DispX/DispY/DispW/DispH/FrmRt/BufLens/Comprs/ByPass/VoDev`；图层通道为 `ChnId/Prio/X/Y/W/H/ChnFrt/FgAlpha/BgAlpha/KeyEn/Color/Show/GDC/Pause/Step/Cache/RevThr/RevCnt`；回写为 `WbcId/En/Src/W/H/Fmt/Comprs/Frt/Depth/SendCnt`。

**异常模式**：设备/图层使能、绑定关系、显示区域、帧率、缓存和 `RevCnt` 可作为显示链路状态和接收帧情况的辅助线索；回写 `SendCnt` 可作为回写发送是否推进的线索，均需结合其他模块确认。

**注意事项**：`VoDev=-1` 表示未绑定；`RevCnt` 在禁用/使能通道时会清零。字段枚举 Y/N、暂停/单帧步进和压缩状态按 PDF 表格解释。

**来源页**：635-638。

## TDE

**用途**：记录 TDE 最近完成任务、最大耗时任务及 job/task 累计状态。

**命令**：`dumpsys tde`

**关键表/字段**：与 VGS 同类，包含 `g_max_job_num/g_max_task_num`、job 的 `seq_no/job_hdl/state/task_num/in_size/out_size/cost_time/hw_time`，操作标志 `copy/fill/resize/bitblit/rotate`，源/目标图像字段 `srcw/srch/src_vir_w/src_vir_h/srcformat/src_rect_x/src_rect_y/src_rect_w/src_rect_h` 和 `dst*`，以及 job/task 的 `success/fail/cancel/all_* /free_num/begin_num/busy_num/procing_num`。

**异常模式**：累计失败/取消、忙和处理中数量、任务耗时可作为 TDE 任务失败、排队或耗时异常的辅助线索，需结合其他模块确认。

**注意事项**：以下录制产生输入/输出数据文件，**非只读，需单独批准**：

```text
./dumpsys tde record /tmp/ 10
```

**来源页**：638-641。

## VDEC

**用途**：记录视频解码属性配置、通道状态和解码 buffer 状态。

**命令**：`./dumpsys vdec`

**关键表/字段**：模块参数为 `vdec_max_chn_num/mb_src`；通道公共属性为 `id/type/width/height/vir_width/vir_height/disp_mode/state/send_mode/send_timeout/set_user_pic/en_user_pic/attach_pool`；视频属性为 `compress/en_dei/en_mv/pixfmt`；通道状态为 `send/send_ok/send_rate/max_input_cnt/left_input_cnt/left_input_size/max_output_cnt/left_output_cnt/left_output_size/err_status`；解码 buffer 状态为 `input_strm_cnt/output_frm_cnt/error_frm_cnt/unused_buf_cnt/put_buf_cnt`。

**异常模式**：`send_ok`、`send_rate`、剩余输入/输出 buffer 和 `err_status` 可作为发送、积压和通道错误的辅助线索；`error_frm_cnt` 可作为错帧线索；`put_buf_cnt` 可辅助检查 buffer 归还，均需结合其他模块确认。

**注意事项**：`mb_src` 的 PDF 枚举为 1 MODULE、2 PRIVATE、3 USER；`send_mode` 为 FRAME 或 STREAM，超时单位为 ms，-1 表示永久阻塞。

**来源页**：641-644。

## VENC

**用途**：记录视频编码通道属性、状态、帧率、序号、取流/释放计数、失败计数、耗时、丢帧和编码配置。

**命令**：`./dumpsys venc`

**关键表/字段**：模块参数为 `venc_max_chn_num/buf_cache/frm_buf_cyc/264_one_pkt/265_one_pkt/jpeg_one_pkt`；通道属性为 `id/width/height/vir_width/vir_height/codec_type/pix_format/buf_count/buf_size/rc_mode/gop_mode/gop/vir_idr_len`；查询状态为 `left_pics/left_strm_bytes/left_strm_frms/cur_packs/left_recv_pics`；RC 为 `start_qp/step_qp/max_qp/min_qp/max_i_qp/min_i_qp/delt_ip_qp/qfactor/qf_min/qf_max` 和 `stat_time/set_min_bps/set_avg_bps/set_max_bps/get_min_bps/get_avg_bps/get_max_bps`；ROI 为 `index/is_intra/abs_qp/qp/x/y/width/height`；OSD 为 `region/total_size/enable/inverse/start_x/start_y/width/height/offset`。

**关键状态字段**：dump status 为 `in_fps/out_fps/seq/snap_set/attach/attach_cnt/attach_size/fail_strm_cnt`；param 为 `crop_mode/src_x/src_y/src_w/src_h/dst_x/dst_y/dst_w/dst_h/fps_en/src_fps_set/dst_fps_set`；frame info1 为 `width/height/vir_w/vir_h/hor_w/ver_h/format/afbc_mode/eos/pts/delay_us/lost`；frame info2 为 `rect_x/rect_y/rect_w/rect_h/send_idx/send_ok_idx/get_idx/get_ok_idx/now_cst_us/avg_cst_us/max_cst_us/drop_cnt`。

**编码配置字段**：base 为 `rotation/mirror`；H.264 为 `intra_pred/tran_mode/chroma_qp/entropy/cabac_init/dblk_dis/dblk_a/dblk_b/full_range`；H.265 为 `cb_qp/cr_qp/scaling_list/dblk_dis/dblk_a/dblk_b/sao_luma/sao_cr/pu_sis_en/full_range`；高级 RC 为 `clear_stat`；超大帧为 `super_frm_mode/rc_priority/Iframe_bits_thr/Pframe_bits_thr`；丢帧为 `frm_lost_open/frm_lost_bps_thr/frm_lost_mode/enc_frm_gaps`；intra refresh 为 `refresh_en/refresh_mode/refresh_num/req_i_qp`；分层 QP 为 `hier_qp_en/hier_qp_delta/hier_frame_num`；去呼吸效应为 `de_breath_en/strength0/strength1`；参考帧为 `base/enhance/en_preb`；slice 分割为 `enable/mode/size`。

**异常模式**：`in_fps/out_fps`、`seq`、`send_idx/send_ok_idx/get_idx/get_ok_idx` 可作为编码输入、输出和取流推进情况的辅助线索；`fail_strm_cnt` 是输出码流获取失败帧数，PDF 说明多包模式下 packet 数不足时会导致获取失败并丢帧；`delay_us`、三类编码耗时和 `drop_cnt/lost` 可作为时延及丢帧线索；RC 的 set/get bitrate 字段可用于比较配置与实际统计，均需结合其他模块确认。

**注意事项**：`crop_mode` 仅支持 PDF 列出的 `none/crop_only/crop_scale` 语义；`afbc_mode` 的 0x0、0x100000、0x200000 含义按 PDF 表格；`clear_stat` 的含义按 PDF 参数说明理解，若执行改变配置的操作需单独批准。其余本节命令仅为 DUMP 查询。

VENC 通道属性表使用 `vir_width/vir_height`；后续 `venc chn frame dump info1` 表使用 `vir_w/vir_h`，两组字段不能混用。PDF 在 frame dump 参数说明中将 `vir_h` 解释为“编码输入帧实际内存宽度”，与字段名及相邻 `vir_w` 含义冲突，属于**PDF 原文疑似笔误**；此处不据此静默改写。

**来源页**：644-651。

## AVS

**用途**：记录全景拼接属性配置和状态，包括 group、输入、输出、通道、pipe 队列和任务状态。

**命令**：`dumpsys avs`

**关键表/字段**：模块参数为 `work_set_size/mb_source`；group 为 `grp_id/mode/enable/pipe_num/is_sync/src_rate/dst_rate`；输入为 `param_source/calib_path/mesh_alpha_path` 或 LUT 的 `lut_data_acc/lut_step_x/lut_step_y/fuse_width/vir_addr[]`；输出为 `proj_mode/center_x/center_y/fov_x/fov_y/ori_yaw/ori_pitch/ori_roll/yaw/pitch/roll`；通道为 `chn_id/enable/width/height/is_compress/dym_range/frm_cnt/depth/src_rate/dst_rate`；状态包含 `cst_time/max_cst_time/prc_into/rty_algn/rty_psh/prc_suc/start_false/stitch_into/stitch_succ`、pipe 发送/丢帧计数和 `get_frm_cnt/rel_frm_cnt/get_frm_rate/lft_in_cnt/lft_out_cnt`。

**异常模式**：拼接成功/失败、pipe 丢帧、队列状态、通道取放帧计数和处理耗时可作为输入同步、拼接处理和输出取帧问题的辅助线索，需结合其他模块确认。

**注意事项**：以下录制会创建目录和多个输入/输出文件，**非只读，需单独批准**：

```text
./dumpsys avs record 0 3 2 /userdata/
```

参数含义和文件名规则以 PDF 为准。以下帧同步调试命令会改变调试状态，**非只读，需单独批准**：

```text
./dumpsys avs open_sync_debug 0
./dumpsys avs close_sync_debug 0
```

这些命令只能在程序正常运行时使用。

**来源页**：651-657。

## GDC

**用途**：记录 GDC 最近任务、最大耗时任务、历史任务参数及累计 job/task 状态。

**命令**：`dumpsys gdc`

**关键表/字段**：模块参数为 `g_max_job_num/g_max_task_num`；recent job 为 `seq_no/job_hdl/correct/correct_ex/q_src/q_src_a/q_pano/q_pano_a/state/task_num/in_size/out_size/cost_time_us/hw_time_us`；latest task 为 `job/task/type/stat/auto_alloc/step_x/step_y`；history task 为 `job/task/type/img_w/img_h/max_cost_us/new_cost_us/new_process_cnt/all_process_cnt`；correct common 为 `all_region/enable/lmf/en_bg_color/bg_color/hor_offset/ver_offset/trap_coef/fan_strength/mount_mode`；correct region 为 `region/view_mode/in_rad/out_rad/pan/tilt/h_zoom/v_zoom/out_x/out_y/out_w/out_h`；query region 为 `query_type/point_num/pano_region`；job/task status 为 `success/fail/cancel/all_job_num/free_num/begin_num/busy_num/procing_num` 和 `all_task_num/free_num/busy_num`。

**异常模式**：任务类型/状态、失败/取消计数、耗时和资源空闲数可作为 GDC 任务执行、失败、排队或耗时异常的辅助线索，需结合其他模块确认；query/correct 标志可用于核对任务类型。

**注意事项**：`view_mode`、query 类型、安装模式和各参数枚举仅采用 PDF 表格中的解释；PDF 未在本节给出 record 命令，不补充录制命令。

**来源页**：657-662。

## ALL

**用途**：记录所有注册模块的配置和状态信息；输出示例包含 `rgn`、`sys`、`venc`、`vo`、`vpss`。

**命令**：

```text
dumpsys all
dumpsys
```

PDF 说明两者均可用于获取所有注册模块信息。

**关键表/字段**：示例中的 SYS 为 `src_mod/src_dev/src_chn/dst_mod/dst_dev/dst_chn/send_cnt`；VENC 包含 `venc_max_chn_num`、通道属性和 `left_pics/left_strm_bytes/left_strm_frms/cur_packs/left_recv_pics`、ROI 字段；VO 包含设备、图层绑定、图层状态、图层通道和 WBC 状态；VPSS 包含组/通道属性、group/chn crop、队列、group pic、通道输出分辨率和旋转属性。

**异常模式**：ALL 输出可作为所有已注册模块整体配置和状态的总览线索；具体异常需按对应模块章节展开并结合其他模块确认。

**注意事项**：只显示已注册模块；不得把 ALL 示例中字段较少的表头当作各模块完整 DUMP 输出。该命令为只读查询。

**来源页**：662-663。
