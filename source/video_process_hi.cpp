#include "video_process_hi.h"
#include "securec.h"
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

videoProcessHi::videoProcessHi() : stVpssChnBufWrap({}), wrap_enable(false), video_stretch_enable(true)
{

}

videoProcessHi::~videoProcessHi()
{

}

int videoProcessHi::init()
{
    hi_mpp_sys_init();
    hi_mpp_vi_init();
    return 0;
}

int videoProcessHi::hi_mpp_sys_init()
{
    ot_vpss_chn_attr chn_attr[3] = {};
    ot_vb_cfg stVbConf = {};
    ot_pic_buf_attr buf_attr = {};
    ot_mpp_sys_cfg stSysConf = { 64 };
    td_u64 u64BlkSize = 0;
    td_s32 s32Ret = TD_FAILURE;

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
    }

    s32Ret = ss_mpi_vb_exit();
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vb_exit failed with %#x", s32Ret);
    }

    s32Ret = ss_mpi_vb_set_cfg(&stVbConf);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vb_set_cfg failed with %#x", s32Ret);
    }

    s32Ret = ss_mpi_vb_init();
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_vb_init failed with %#x", s32Ret);
    }

    s32Ret = ss_mpi_sys_set_cfg(&stSysConf);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_set_cfg failed with %#x", s32Ret);
    }

    s32Ret = ss_mpi_sys_init();
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_init failed with %#x", s32Ret);
    }

    return 0;
}

int videoProcessHi::hi_mpp_vi_init()
{
    // set vi_vpss_mode: should be before vpss init
    td_s32 s32Ret = TD_FAILURE;
    ot_vi_dev   ViDev  = 0;
    ot_vi_pipe  ViPipe = 0;
    ot_vi_chn   ViChn  = 0;
    ot_vi_vpss_mode_type ViVpssMode = OT_VI_OFFLINE_VPSS_ONLINE;
    s32Ret = set_vi_vpss_mode(ViDev, ViPipe, ViChn, ViVpssMode);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "set_vi_vpss_mode failed with %#x", s32Ret);
        return s32Ret;
    }
    LOGGER_NOTICE(HIMPP, "set_vi_vpss_mode success");

    // init vi module
    s32Ret = init_vi_module(ViDev, ViPipe, ViChn);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "init_vi_module failed with %#x", s32Ret);
        return s32Ret;
    }
    LOGGER_NOTICE(HIMPP, "init_vi_module success");

    return 0;
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
    td_s32              i;
    td_s32              s32Ret;
    ot_isp_ctrl_param   stIspCtrlParam;
    td_u32              u32FrameRate;
    // SAMPLE_VI_CONFIG_S  stViConfig;
    td_s32              s32ViNum;
    ot_vi_pipe          ViPipeTmp; 
    ot_vi_vpss_mode     stVIVPSSMode;
    // SAMPLE_VI_INFO_S*   pstViInfo = TD_NULL;

    s32Ret = ss_mpi_sys_get_vi_vpss_mode(&stVIVPSSMode);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_get_vi_vpss_mode failed with %#x", s32Ret);
        return s32Ret;
    }

    stVIVPSSMode.mode[0] = ViVpssMode;

    s32Ret = ss_mpi_sys_set_vi_vpss_mode(&stVIVPSSMode);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_set_vi_vpss_mode failed with %#x", s32Ret);
        return s32Ret;
    }

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
    s32Ret = init_vi_process(ViVpssMode, ViPipe);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "Init VI err for %#x!", s32Ret);
        return s32Ret;
    }
	LOGGER_NOTICE(HIMPP, "init vi process success");
    return TD_SUCCESS;
}

td_s32 videoProcessHi::init_vi_process(ot_vi_vpss_mode_type ViVpssMode, ot_vi_pipe ViPipe)
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
    return TD_SUCCESS;
}