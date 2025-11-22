#include "video_process_hi.h"
#include <securec.h>
#include <ot_common_sys.h>
#include <ss_mpi_sys.h>
#include <ot_common_vb.h>
#include <ss_mpi_vb.h>
#include "logger.h"
#include <ot_buffer.h>
#include <ot_common_isp.h>
#include <ot_common_vi.h>
#include <ss_mpi_vi.h>
#include <ot_common_isp.h>
#include <ss_mpi_isp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ot_mipi_rx.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ss_mpi_ae.h>
#include <ot_common_ae.h>
#include <ot_common_3a.h>
#include <ot_common_awb.h>
#include <ss_mpi_awb.h>
#include <sys/prctl.h>

videoProcessHi::videoProcessHi() : stVpssChnBufWrap({}), wrap_enable(false), video_stretch_enable(true)
{

}

videoProcessHi::~videoProcessHi()
{

}

int videoProcessHi::init()
{
    int ret = TD_SUCCESS;
    ret = hi_mpp_sys_init();
    if (ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "hi_mpp_sys_init failed!");
        goto Release;
    }

    ret = hi_mpp_vi_init();
    if (ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "hi_mpp_vi_init failed!");
        goto Release;
    }

    // init vpss module

Release:
    return ret;
}

td_s32 videoProcessHi::hi_mpp_sys_init()
{
    ot_vpss_chn_attr chn_attr[3] = {};
    ot_vb_cfg stVbConf = {};
    ot_pic_buf_attr buf_attr = {};
    ot_mpp_sys_cfg stSysConf = { 64 };
    td_u64 u64BlkSize = 0;
    td_s32 s32Ret = TD_SUCCESS;

    buf_attr.align = OT_DEFAULT_ALIGN;
    buf_attr.bit_width = OT_DATA_BIT_WIDTH_8;
    buf_attr.compress_mode = OT_COMPRESS_MODE_NONE;
    buf_attr.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    buf_attr.video_format = OT_VIDEO_FORMAT_LINEAR;

    comm_vpss_get_default_chn_attr(&chn_attr[0]);
    comm_vpss_get_wrap_cfg(&chn_attr[0], OT_VI_OFFLINE_VPSS_ONLINE, 1700, &stVpssChnBufWrap);

    // 初始化MPP视频缓存池
    stVbConf.max_pool_cnt = 40;
    if(wrap_enable) {
        stVbConf.common_pool[0].blk_cnt = 1;
        stVbConf.common_pool[0].blk_size = stVpssChnBufWrap.buf_size;
    } else {
        //主码流 VI-offline-VPSS 2; VPSS-online-VENC 1; 修改分辨率时 VPSS-VGS-VENC 1;
        buf_attr.width = VI_WIDTH;
        buf_attr.height = VI_HEIGHT;
		u64BlkSize = ot_common_get_pic_buf_size(&buf_attr);
        stVbConf.common_pool[0].blk_size = u64BlkSize;
        stVbConf.common_pool[0].blk_cnt = 2;      
    }

    //次码流 VPSS-online-VENC 1; 多分一个帧率才稳定，没弄懂
    buf_attr.width = 720;
    buf_attr.height = 576;
    u64BlkSize = ot_common_get_pic_buf_size(&buf_attr);
    stVbConf.common_pool[1].blk_size = u64BlkSize;
    stVbConf.common_pool[1].blk_cnt = 2;
   
    //移动侦测&智能分析 1;
    buf_attr.width = IVP_SMD_W;
    buf_attr.height = IVP_SMD_H;
    // buf_attr.pixel_format = OT_PIXEL_FORMAT_YUV_400;
    u64BlkSize = ot_common_get_pic_buf_size(&buf_attr);
    stVbConf.common_pool[2].blk_size = u64BlkSize;
    stVbConf.common_pool[2].blk_cnt = 2;

    s32Ret = ss_mpi_sys_exit();
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_exit failed with %#x", s32Ret);
        goto Release;
    }

    s32Ret = ss_mpi_vb_exit();
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vb_exit failed with %#x", s32Ret);
        goto Release;
    }

    s32Ret = ss_mpi_vb_set_cfg(&stVbConf);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vb_set_cfg failed with %#x", s32Ret);
        goto Release;
    }

    s32Ret = ss_mpi_vb_init();
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vb_init failed with %#x", s32Ret);
        goto Release;
    }

    s32Ret = ss_mpi_sys_set_cfg(&stSysConf);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_set_cfg failed with %#x", s32Ret);
        goto Release;
    }

    s32Ret = ss_mpi_sys_init();
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_init failed with %#x", s32Ret);
        goto Release;
    }

Release:
    return s32Ret;
}

td_s32 videoProcessHi::hi_mpp_vi_init()
{
    // set vi_vpss_mode: should be before vpss init
    td_s32 s32Ret = TD_SUCCESS;
    ot_vi_dev   ViDev  = 0;
    ot_vi_pipe  ViPipe = 0;
    ot_vi_chn   ViChn  = 0;
    ot_vi_vpss_mode_type ViVpssMode = OT_VI_OFFLINE_VPSS_ONLINE;
    s32Ret = set_vi_vpss_mode(ViDev, ViPipe, ViChn, ViVpssMode);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "set_vi_vpss_mode failed!");
        goto Release;
    }
    LOGGER_NOTICE(HIMPP, "set_vi_vpss_mode success");

    // init vi module
    s32Ret = init_vi_module(ViDev, ViPipe, ViChn);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "init_vi_module failed with %#x", s32Ret);
        goto Release;
    }
    LOGGER_NOTICE(HIMPP, "init_vi_module success");

Release:
    return s32Ret;
}

td_void videoProcessHi::comm_vpss_get_default_chn_attr(ot_vpss_chn_attr *chn_attr)
{
    chn_attr->mirror_en                 = TD_FALSE;
    chn_attr->flip_en                   = TD_FALSE;
    chn_attr->border_en                 = TD_FALSE;
    chn_attr->width                     = VIDEO_STRETCH_WIDTH;
    chn_attr->height                    = VIDEO_STRETCH_HEIGHT;
    chn_attr->depth                     = 0;
    chn_attr->chn_mode                  = OT_VPSS_CHN_MODE_USER;
    chn_attr->video_format              = OT_VIDEO_FORMAT_LINEAR;
    chn_attr->dynamic_range             = OT_DYNAMIC_RANGE_SDR8;
    chn_attr->pixel_format              = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    chn_attr->compress_mode             = OT_COMPRESS_MODE_SEG_COMPACT;
    chn_attr->aspect_ratio.mode         = OT_ASPECT_RATIO_NONE;
    chn_attr->frame_rate.src_frame_rate = -1;
    chn_attr->frame_rate.dst_frame_rate = -1;
}

td_s32 videoProcessHi::comm_vpss_get_wrap_cfg(ot_vpss_chn_attr chn_attr[OT_VPSS_MAX_PHYS_CHN_NUM],
    ot_vi_vpss_mode_type mode, td_u32 full_lines_std, ot_vpss_chn_buf_wrap_attr *wrap_attr)
{
    ot_vpss_venc_wrap_param wrap_param = {};
    td_u32 buf_line;
    td_u32 wrap_buf_size;
    ot_pic_buf_attr buf_attr;
    td_s32 s32Ret = TD_FAILURE;

    wrap_attr->enable = TD_FALSE;

    wrap_param.frame_rate = 25; /* frame_rate 30 */
    if (chn_attr[0].frame_rate.src_frame_rate > 0 && chn_attr[0].frame_rate.dst_frame_rate > 0) {
        /* frame_rate 30 */
        wrap_param.frame_rate = chn_attr[0].frame_rate.dst_frame_rate * 30 / chn_attr[0].frame_rate.src_frame_rate;
    }
    if (mode == OT_VI_ONLINE_VPSS_ONLINE) {
        wrap_param.all_online = TD_TRUE;
    }
    wrap_param.full_lines_std = full_lines_std;
    wrap_param.large_stream_size.width = chn_attr[0].width;
    wrap_param.large_stream_size.height = chn_attr[0].height;
    wrap_param.small_stream_size.width = 720;
    wrap_param.small_stream_size.height = 480;
    s32Ret = ss_mpi_sys_get_vpss_venc_wrap_buf_line(&wrap_param, &buf_line);
    if(TD_SUCCESS !=s32Ret)
    {
        LOGGER_ERROR(HIMPP, "get buf line failed s32Ret:%#x", s32Ret);
        return TD_FAILURE;
    }

    buf_attr.width = chn_attr[0].width;
    buf_attr.height = chn_attr[0].height;
    buf_attr.bit_width = OT_DATA_BIT_WIDTH_8;
    buf_attr.pixel_format = chn_attr[0].pixel_format;
    buf_attr.compress_mode = chn_attr[0].compress_mode;
    buf_attr.align = OT_DEFAULT_ALIGN;
    buf_attr.video_format = chn_attr[0].video_format;
    wrap_buf_size = ot_comm_get_vpss_venc_wrap_buf_size(&buf_attr, buf_line);

    /* out */
    wrap_attr->enable = TD_TRUE;
    wrap_attr->buf_line = buf_line;
    wrap_attr->buf_size = wrap_buf_size;
    LOGGER_INFO(LOG_CAT_HIMPP, "wrap online is %u, buf line is %u, buf size is %u", wrap_param.all_online, buf_line, wrap_buf_size);
    return TD_SUCCESS;
}

td_s32 videoProcessHi::set_vi_vpss_mode(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn, 
ot_vi_vpss_mode_type ViVpssMode)
{
    td_s32 s32Ret;
    ot_vi_vpss_mode stVIVPSSMode;
    size_t mode_count = sizeof(stVIVPSSMode.mode) / sizeof(stVIVPSSMode.mode[0]);
    int has_zero = 0;

    s32Ret = ss_mpi_sys_get_vi_vpss_mode(&stVIVPSSMode);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_get_vi_vpss_mode failed with %#x", s32Ret);
        return s32Ret;
    }

    /* 检查是否存在 0（可能是不合法的默认值）*/
    for (size_t i = 0; i < mode_count; ++i) {
        if (stVIVPSSMode.mode[i] == 0) {
            has_zero = 1;
            break;
        }
    }

    if (has_zero) {
        /* 如果读到有 0，说明可能为未初始化状态：为避免驱动拒绝，给所有 pipe 设成传入的合法模式 */
        for (size_t i = 0; i < mode_count; ++i) {
            if( stVIVPSSMode.mode[i] == 0)
                stVIVPSSMode.mode[i] = ViVpssMode;
        }
        LOGGER_INFO(HIMPP, "detected zero entries, set all modes to %d", ViVpssMode);
    } else {
        /* 否则只修改目标 pipe（保留其它 pipe 的已有配置）*/
        if ((td_u32)ViPipe >= mode_count) {
            LOGGER_ERROR(HIMPP, "ViPipe index %d out of range (count=%zu)", ViPipe, mode_count);
            return -1;
        }
        stVIVPSSMode.mode[ViPipe] = ViVpssMode;
        LOGGER_INFO(HIMPP, "only change pipe %d to mode %d", ViPipe, ViVpssMode);
    }

    /* 调用 set 并打印尝试写入的值以便排查 */
    // {
    //     char buf[128] = {0};
    //     int off = 0;
    //     for (size_t i = 0; i < mode_count; ++i) {
    //         off += snprintf(buf + off, sizeof(buf) - off, "mode[%zu]=%d ", i, stVIVPSSMode.mode[i]);
    //         if (off >= (int)sizeof(buf)) break;
    //     }
    //     LOGGER_NOTICE(HIMPP, "trying to set vi_vpss_mode: %s", buf);
    // }

    s32Ret = ss_mpi_sys_set_vi_vpss_mode(&stVIVPSSMode);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_set_vi_vpss_mode failed with %#x", s32Ret);
        return s32Ret;
    }

    LOGGER_NOTICE(HIMPP, "ss_mpi_sys_set_vi_vpss_mode success");
    return TD_SUCCESS;
}

td_s32 videoProcessHi::init_vi_module(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn)
{
    td_s32              s32Ret;
    ot_isp_ctrl_param    stIspCtrlParam;
    td_u32              u32FrameRate;
	
    // set wrap mode
    ot_vi_vpss_mode_type ViVpssMode = OT_VI_ONLINE_VPSS_ONLINE;
    if (video_stretch_enable) {
        ViVpssMode = OT_VI_OFFLINE_VPSS_ONLINE;
        ot_vi_pipe_buf_wrap_attr wrap_attr;
        wrap_attr.buf_line = 340; // VI_HEIGHT/2;
        wrap_attr.enable = TD_TRUE;
        s32Ret = ss_mpi_vi_set_pipe_buf_wrap_attr(ViPipe, &wrap_attr);
        if (s32Ret != TD_SUCCESS) {
            LOGGER_ERROR(HIMPP, "vi set pipe(%d) buf_wrap_attr failed with %#x!", ViPipe, s32Ret);
            return s32Ret;
        } else {
            LOGGER_INFO(HIMPP, "wrap_attr.buf_line %d", wrap_attr.buf_line);
        }
    }
    
    // init vi process: vi enable dev, bind-create-start pipe, enable chn
    s32Ret = init_vi_process(ViVpssMode, ViDev, ViPipe, ViChn);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "Init VI err for %#x!", s32Ret);
        return s32Ret;
    }
	LOGGER_NOTICE(HIMPP, "init vi process success");
    return TD_SUCCESS;
}

td_s32 videoProcessHi::init_vi_process(ot_vi_vpss_mode_type ViVpssMode, ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn)
{
    td_s32              s32Ret;
    ot_isp_ctrl_param   stIspCtrlParam;
    td_u32              u32FrameRate;

    s32Ret = ss_mpi_isp_get_ctrl_param(ViPipe, &stIspCtrlParam);
    if (TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "mpi isp get ctrl_param failed with %d!", s32Ret);
        return s32Ret;
    }

    stIspCtrlParam.stat_interval  = SENSOR_FRAME_RATE/30;
    s32Ret = ss_mpi_isp_set_ctrl_param(ViPipe, &stIspCtrlParam);
    if (TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "mpi isp set ctrl_param failed with %d!", s32Ret);
        return s32Ret;
    }

    // start vi pipe
    s32Ret = comm_vi_start_vi(ViDev, ViPipe, ViChn);
    if (TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "comm_vi_start_vi failed with %d!", s32Ret);
        return s32Ret;
    }

    return TD_SUCCESS;
}

td_s32 videoProcessHi::comm_vi_start_vi(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn)
{
    td_s32 s32Ret;

    // 1.init mipi
    s32Ret = comm_vi_start_mipi();
    if(TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "comm_vi_start_mipi failed with %d!", s32Ret);
        return s32Ret;
    }

    // enable start vi pipe and channel
    s32Ret = comm_vi_create_vi(ViDev, ViPipe, ViChn);
    if(TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "comm_vi_create_vi failed with %d!", s32Ret);
        return s32Ret;
    }

    s32Ret = comm_vi_create_isp(ViPipe);
    if(TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "comm_vi_create_isp failed with %d!", s32Ret);
        return s32Ret;
    }

    return TD_SUCCESS;
}

td_s32 videoProcessHi::comm_vi_start_mipi()
{
    td_s32 s32Ret;
    td_s32 fd;
    combo_dev_t devno = 0;
    sns_clk_source_t snsDev = 0;
    combo_dev_attr_t stcomboDevAttr;

    lane_divide_mode_t lane_divide_mode = LANE_DIVIDE_MODE_0;
    fd = open(MIPI_DEV_NODE, O_RDWR);
    if(fd < 0) {
        LOGGER_ERROR(HIMPP, "open %s failed!", MIPI_DEV_NODE);
        return TD_FAILURE;
    }

    // set mipi hs mode
    s32Ret = ioctl(fd, OT_MIPI_SET_HS_MODE, &lane_divide_mode);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_SET_HS_MODE failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }

    // enable mipi clock
    s32Ret = ioctl(fd, OT_MIPI_ENABLE_MIPI_CLOCK, &devno);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_ENABLE_MIPI_CLOCK failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }

    // reset mipi
    s32Ret = ioctl(fd, OT_MIPI_RESET_MIPI, &devno);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_RESET_MIPI failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }

    // enable sensor clock
    s32Ret = ioctl(fd, OT_MIPI_ENABLE_SENSOR_CLOCK, &snsDev);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_ENABLE_SENSOR_CLOCK failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }

    // reset sensor
    s32Ret = ioctl(fd, OT_MIPI_RESET_SENSOR, &snsDev);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_RESET_SENSOR failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }
    
    // set mipi dev attr (SC4336P)
    stcomboDevAttr.devno = devno;
    stcomboDevAttr.input_mode = INPUT_MODE_MIPI;
    stcomboDevAttr.data_rate = MIPI_DATA_RATE_X1;
    stcomboDevAttr.img_rect = {0, 0, SENSOR_MAX_WIDTH, SENSOR_MAX_HRIGHT};
    stcomboDevAttr.mipi_attr = {
        DATA_TYPE_RAW_10BIT,
        OT_MIPI_WDR_MODE_NONE,
    };
    s32Ret = ioctl(fd, OT_MIPI_SET_DEV_ATTR, &stcomboDevAttr);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_SET_DEV_ATTR failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }

    // unreset mipi
    s32Ret = ioctl(fd, OT_MIPI_UNRESET_MIPI, &devno);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_UNRESET_MIPI failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }

    // unreset sensor
    s32Ret = ioctl(fd, OT_MIPI_UNRESET_SENSOR, &snsDev);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_UNRESET_SENSOR failed with %#x", s32Ret);
        close(fd);
        return TD_FAILURE;
    }

    close(fd);
    return TD_SUCCESS;
}

td_s32 videoProcessHi::comm_vi_create_vi(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn)
{
    td_s32 s32Ret;
    ot_vi_dev_attr stViDevAttr = {
        .intf_mode = OT_VI_INTF_MODE_MIPI,
        .work_mode = OT_VI_WORK_MODE_MULTIPLEX_1,
        .scan_mode = OT_VI_SCAN_PROGRESSIVE,
        .ad_chn_id = { -1, -1, -1, -1 },
        .data_seq = OT_VI_DATA_SEQ_YUYV,
        .sync_cfg = {
            .vsync = OT_VI_VSYNC_PULSE,
            .vsync_neg = OT_VI_VSYNC_NEG_HIGH,
            .hsync = OT_VI_HSYNC_PULSE,
            .hsync_neg = OT_VI_HSYNC_NEG_HIGH,
            .vsync_valid = OT_VI_VSYNC_NORM_PULSE,
            .vsync_valid_neg = OT_VI_VSYNC_VALID_NEG_HIGH,
            .timing_blank = {0, SENSOR_MAX_WIDTH, 0, 0, SENSOR_MAX_HRIGHT, 0, 0, 0, 0}
        },
        .data_type = OT_VI_DATA_TYPE_RAW,
        .data_reverse = TD_FALSE,
        .in_size = {SENSOR_MAX_WIDTH, SENSOR_MAX_HRIGHT},
        .data_rate = OT_DATA_RATE_X1
    };
    // ot_vi_bind_pipe  stDevBindPipe = {};
    ot_vi_pipe_attr  stPipeAttr = {
        .pipe_bypass_mode = OT_VI_PIPE_BYPASS_NONE,
        .isp_bypass = TD_FALSE,
        .size = {SENSOR_MAX_WIDTH, SENSOR_MAX_HRIGHT},
        .pixel_format = OT_PIXEL_FORMAT_RGB_BAYER_10BPP,
        .compress_mode = OT_COMPRESS_MODE_NONE,
        .frame_rate_ctrl = {-1, -1}
    };

    ot_vi_chn_attr stChnAttr = {
        .size = {SENSOR_MAX_WIDTH, SENSOR_MAX_HRIGHT},
        .pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420,
        .dynamic_range = OT_DYNAMIC_RANGE_SDR8,
        .video_format = OT_VIDEO_FORMAT_LINEAR,
        .compress_mode = OT_COMPRESS_MODE_NONE,
        .mirror_en = TD_FALSE,
        .flip_en = TD_FALSE,
        .depth = 0,
        .frame_rate_ctrl = {-1, -1}
    };
    ot_3dnr_attr dnr_attr = {};

    // vi enable dev
    s32Ret = ss_mpi_vi_set_dev_attr(ViDev, &stViDevAttr);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vi_set_dev_attr failed with %#x", s32Ret);
        return s32Ret;
    }

    s32Ret = ss_mpi_vi_enable_dev(ViDev);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vi_enable_dev failed with %#x", s32Ret);
        return s32Ret;
    }

    // vi bind dev to pipe interface 单sensor
    s32Ret = ss_mpi_vi_bind(ViDev, ViPipe);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vi_bind failed with %#x", s32Ret);
        return s32Ret;
    }

    // create vi pipe, then start it
    s32Ret = ss_mpi_vi_create_pipe(ViDev, &stPipeAttr);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vi_create_pipe failed with %#x", s32Ret);
        return s32Ret;
    }

    // start vi pipe
    s32Ret = ss_mpi_vi_start_pipe(ViPipe);
    if (s32Ret != TD_SUCCESS)
    {
        ss_mpi_vi_destroy_pipe(ViPipe);
        LOGGER_ERROR(HIMPP, "ss_mpi_vi_start_pipe failed with %#x!", s32Ret);
        return s32Ret;
    }
    
    // enable vi channel
    s32Ret = ss_mpi_vi_set_chn_attr(ViPipe, ViChn, &stChnAttr);
    if (s32Ret != TD_SUCCESS)
    {
        ss_mpi_vi_destroy_pipe(ViPipe);
        LOGGER_ERROR(HIMPP, "ss_mpi_vi_start_pipe failed with %#x!", s32Ret);
        return s32Ret;
    }
    
    // s32Ret = ss_mpi_vi_enable_chn(ViPipe, ViChn);
    // if (s32Ret != TD_SUCCESS)
    // {
    //     LOGGER_ERROR(HIMPP, "vi enable chn failed with %#x!\n", s32Ret);
    //     return s32Ret;
    // }

    ss_mpi_vi_get_pipe_3dnr_attr(ViPipe, &dnr_attr);
    
    dnr_attr.enable = TD_TRUE;
    dnr_attr.compress_mode = OT_COMPRESS_MODE_FRAME;
    s32Ret = ss_mpi_vi_set_pipe_3dnr_attr(ViPipe, &dnr_attr);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP,"vi pipe(%d) set 3dnr_attr failed!", ViPipe);
        return s32Ret;
    }

    // fail to StopViPipe , StopDev

    return TD_SUCCESS;
}

td_s32 videoProcessHi::comm_vi_create_isp(ot_vi_pipe ViPipe)
{
    td_s32 s32Ret;
    ot_isp_3a_alg_lib stAeLib = { ViPipe, OT_AE_LIB_NAME };
    ot_isp_3a_alg_lib stAwbLib = { ViPipe, OT_AWB_LIB_NAME };
    ot_isp_pub_attr stPubAttr = {
        {0, 0, SENSOR_MAX_WIDTH, SENSOR_MAX_HRIGHT},
        { SENSOR_MAX_WIDTH, SENSOR_MAX_HRIGHT },
        30,
        OT_ISP_BAYER_GBRG,
        OT_WDR_MODE_NONE,
        0, TD_FALSE, TD_FALSE,
        {TD_FALSE, {0, 0, SENSOR_MAX_WIDTH, SENSOR_MAX_HRIGHT}}
    };

    // ae register
    s32Ret = ss_mpi_ae_register(ViPipe, &stAeLib);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "ss_mpi_ae_register failed with %#x!", s32Ret);
        return s32Ret;
    }

    // awb_register
    s32Ret = ss_mpi_awb_register(ViPipe, &stAwbLib);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "ss_mpi_ae_register failed with %#x!", s32Ret);
        return s32Ret;
    }
    
    s32Ret = ss_mpi_isp_mem_init(ViPipe);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"Init Ext memory failed with %#x!", s32Ret);
        return s32Ret;
    }

    s32Ret = ss_mpi_isp_set_pub_attr(ViPipe, &stPubAttr);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"SetPubAttr failed with %#x!", s32Ret);
        return s32Ret;
    }

    s32Ret = ss_mpi_isp_init(ViPipe);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"ss_mpi_isp_init failed with %#x!", s32Ret);
        return s32Ret;
    }
    
    s32Ret = comm_isp_run(ViPipe);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"comm_isp_run failed with %#x!", s32Ret);
        return s32Ret;
    }    

    return TD_SUCCESS;
}

td_s32 videoProcessHi::comm_isp_task(ot_isp_dev IspDev)
{
    td_s32 s32Ret = TD_SUCCESS;
    td_char szThreadName[20] = {};
    snprintf(szThreadName, 20, "ISP%d_RUN", IspDev);
    prctl(PR_SET_NAME, szThreadName, 0,0,0);

    LOGGER_NOTICE(HIMPP, "ISP Dev %d running !", IspDev);
    s32Ret = ss_mpi_isp_run(IspDev);
    if (TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "HI_MPI_ISP_Run failed with %#x!", s32Ret);
    }

    return s32Ret;
}

td_s32 videoProcessHi::comm_isp_run(ot_isp_dev IspDev)
{
    td_s32 s32Ret = TD_SUCCESS;

    ispThread = std::thread(&videoProcessHi::comm_isp_task, this, IspDev);

    return s32Ret;
}