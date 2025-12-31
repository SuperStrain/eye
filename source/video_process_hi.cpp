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
#include <ss_mpi_vpss.h>
#include <ss_mpi_sys_bind.h>
#include <sensor/sc4336p/sc4336p_cmos.h>


namespace hiMppMedia {

videoProcessHi::videoProcessHi() : stVpssChnBufWrap({}), wrap_enable(false), 
video_stretch_enable(false)
{

}

videoProcessHi::~videoProcessHi()
{

}

int videoProcessHi::init()
{
    int ret = TD_SUCCESS;

	ot_vpss_grp VpssGrp = 0;
    ot_vpss_chn VpssChn = 0;
    ot_vi_pipe  ViPipe = 0;
    ot_vi_chn   ViChn  = 0;
    td_bool     abChnEnable[OT_VPSS_MAX_PHYS_CHN_NUM] = {TD_TRUE, TD_TRUE, TD_TRUE};

    ret = hi_mpp_sys_init();
    if (ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "hi_mpp_sys_init failed!");
        goto Release;
    }

    ret = hi_mpp_vi_init(ViPipe, ViChn);
    if (ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "hi_mpp_vi_init failed!");
        goto Release;
    }

    // init vpss module
    ret = init_vpss_module(VpssGrp, abChnEnable);
    if (ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "init_vpss_module failed!");
        goto Release;
    }

    // bind vi to vpss
    ret = bind_vi_vpss(ViPipe, ViChn, VpssGrp, VpssChn);
    if(ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "bind vi to vpss failed, ret %d", ret);
        goto Release;
    }

    // ircut_init ?

    // mdetect_init_ivp 移动侦测？

    // venc module

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

    if(wrap_enable) {
        comm_vpss_get_wrap_cfg(&chn_attr[0], OT_VI_OFFLINE_VPSS_ONLINE, 1700, &stVpssChnBufWrap);
    }

    // 初始化MPP视频缓存池
    stVbConf.max_pool_cnt = 128;
    if(wrap_enable) {
        stVbConf.common_pool[0].blk_cnt = 1;
        stVbConf.common_pool[0].blk_size = stVpssChnBufWrap.buf_size;
    } else {
        // 主码流
        buf_attr.width = VI_WIDTH0;
        buf_attr.height = VI_HEIGHT0;
		u64BlkSize = ot_common_get_pic_buf_size(&buf_attr);
        stVbConf.common_pool[0].blk_size = u64BlkSize;
        stVbConf.common_pool[0].blk_cnt = 3;      
    }

    // 次码流
    buf_attr.width = VI_WIDTH1;
    buf_attr.height = VI_HEIGHT1;
    u64BlkSize = ot_common_get_pic_buf_size(&buf_attr);
    stVbConf.common_pool[1].blk_size = u64BlkSize;
    stVbConf.common_pool[1].blk_cnt = 2;
   
    // 算法使用
    buf_attr.width = VI_WIDTH2;
    buf_attr.height = VI_HEIGHT2;
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

    // ss_mpi_vb_set_supplement_cfg

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

td_s32 videoProcessHi::hi_mpp_vi_init(ot_vi_pipe ViPipe, ot_vi_chn ViChn)
{
    // set vi_vpss_mode: should be before vpss init
    td_s32 s32Ret = TD_SUCCESS;
    ot_vi_dev   ViDev  = 0;
    ot_vi_vpss_mode_type ViVpssMode = OT_VI_ONLINE_VPSS_ONLINE;
    s32Ret = set_vi_vpss_mode(ViDev, ViPipe, ViChn, ViVpssMode);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "set_vi_vpss_mode failed!");
        goto Release;
    }
    LOGGER_NOTICE(HIMPP, "set_vi_vpss_mode success");

    // init vi module
    s32Ret = init_vi_module(ViDev, ViPipe, ViChn);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "init_vi_module failed with %d", s32Ret);
        goto Release;
    }
    LOGGER_NOTICE(HIMPP, "init_vi_module success");

Release:
    return s32Ret;
}

td_s32 videoProcessHi::init_vpss_module(ot_vpss_grp VpssGrp, td_bool *abChnEnable)
{
    td_s32 s32Ret = TD_SUCCESS;
    ot_vpss_grp_attr stVpssGrpAttr = { };
    ot_vpss_ext_chn_attr stVpssExtChnAttr;
    ot_vi_pipe         ViPipe       = 0;
    ot_vi_chn          ViChn        = 0;

    int resList[][2] = {
            {SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT},
            {VI_WIDTH0, VI_HEIGHT0},
            {VI_WIDTH1, VI_HEIGHT1},
            {VI_WIDTH2, VI_HEIGHT2},
            {400,      224     },
    };

    // create vpss group
	stVpssGrpAttr.max_width = resList[0][0];
	stVpssGrpAttr.max_height = resList[0][1];
	stVpssGrpAttr.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
	stVpssGrpAttr.dynamic_range = OT_DYNAMIC_RANGE_SDR8;
	stVpssGrpAttr.frame_rate.src_frame_rate = -1;
    stVpssGrpAttr.frame_rate.dst_frame_rate = -1;
	s32Ret = ss_mpi_vpss_create_grp(VpssGrp, &stVpssGrpAttr);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "ss_mpi_vpss_create_grp(grp:%d) failed with %#x!", VpssGrp, s32Ret);
        return s32Ret;
    }

    ot_vpss_chn_attr stVpssChnAttr[OT_VPSS_MAX_PHYS_CHN_NUM] = { };
    ot_vpss_chn VpssChn;
    // the first stream, enable vpss channel 0
    VpssChn = vpssChn0;
    stVpssChnAttr[VpssChn].chn_mode      = OT_VPSS_CHN_MODE_USER;
    stVpssChnAttr[VpssChn].width       = resList[1][0];
    stVpssChnAttr[VpssChn].height      = resList[1][1];
    stVpssChnAttr[VpssChn].video_format = OT_VIDEO_FORMAT_LINEAR;
    stVpssChnAttr[VpssChn].pixel_format  = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssChnAttr[VpssChn].compress_mode = OT_COMPRESS_MODE_SEG_COMPACT;
    stVpssChnAttr[VpssChn].dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    stVpssChnAttr[VpssChn].frame_rate.src_frame_rate = -1;
    stVpssChnAttr[VpssChn].frame_rate.dst_frame_rate = -1;	
    stVpssChnAttr[VpssChn].mirror_en = TD_FALSE;
    stVpssChnAttr[VpssChn].flip_en = TD_FALSE;
    stVpssChnAttr[VpssChn].depth = 0;
    stVpssChnAttr[VpssChn].aspect_ratio.mode = OT_ASPECT_RATIO_NONE;
    
    s32Ret = enable_vpss_chn(VpssGrp, VpssChn, &stVpssChnAttr[VpssChn], TD_NULL);
    if (TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "Enable vpss chn %d failed!", VpssChn);
	    abChnEnable[VpssChn] = TD_FALSE;
        return s32Ret;
    } else {
	    abChnEnable[VpssChn] = TD_TRUE;
    }    

    // the second stream, enable vpss channel 1
    VpssChn = vpssChn1;
    stVpssChnAttr[VpssChn].chn_mode     = OT_VPSS_CHN_MODE_USER;
    stVpssChnAttr[VpssChn].width      = resList[2][0];
    stVpssChnAttr[VpssChn].height     = resList[2][1];
    stVpssChnAttr[VpssChn].video_format = OT_VIDEO_FORMAT_LINEAR;
    stVpssChnAttr[VpssChn].pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssChnAttr[VpssChn].dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    stVpssChnAttr[VpssChn].compress_mode = OT_COMPRESS_MODE_NONE;
    stVpssChnAttr[VpssChn].frame_rate.src_frame_rate = -1;
    stVpssChnAttr[VpssChn].frame_rate.dst_frame_rate = -1;    
    stVpssChnAttr[VpssChn].mirror_en = TD_FALSE;
    stVpssChnAttr[VpssChn].flip_en = TD_FALSE;
    stVpssChnAttr[VpssChn].depth = 0;
    stVpssChnAttr[VpssChn].aspect_ratio.mode = OT_ASPECT_RATIO_NONE;
    s32Ret = enable_vpss_chn(VpssGrp, VpssChn, &stVpssChnAttr[VpssChn], TD_NULL);
    if (TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "Enable vpss chn %d failed!", VpssChn);
        abChnEnable[VpssChn] = TD_FALSE;
        return s32Ret;
    } else {
        abChnEnable[VpssChn] = TD_TRUE;
    }

    VpssChn = vpssChn2;
    stVpssChnAttr[VpssChn].chn_mode     = OT_VPSS_CHN_MODE_USER;
    stVpssChnAttr[VpssChn].width      = resList[3][0];
    stVpssChnAttr[VpssChn].height     = resList[3][1];
    stVpssChnAttr[VpssChn].video_format = OT_VIDEO_FORMAT_LINEAR;
    stVpssChnAttr[VpssChn].pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stVpssChnAttr[VpssChn].dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    stVpssChnAttr[VpssChn].compress_mode = OT_COMPRESS_MODE_NONE;
    stVpssChnAttr[VpssChn].frame_rate.src_frame_rate = -1;
    stVpssChnAttr[VpssChn].frame_rate.dst_frame_rate = -1;    
    stVpssChnAttr[VpssChn].mirror_en = TD_FALSE;
    stVpssChnAttr[VpssChn].flip_en = TD_FALSE;
    stVpssChnAttr[VpssChn].depth = 2;
    stVpssChnAttr[VpssChn].aspect_ratio.mode = OT_ASPECT_RATIO_NONE;
    s32Ret = enable_vpss_chn(VpssGrp, VpssChn, &stVpssChnAttr[VpssChn], TD_NULL);
    if (TD_SUCCESS != s32Ret)
    {
        LOGGER_ERROR(HIMPP, "Enable vpss chn %d failed!", VpssChn);
        abChnEnable[VpssChn] = TD_FALSE;
        return s32Ret;
    } else {
        abChnEnable[VpssChn] = TD_TRUE;
    }

    s32Ret = ss_mpi_vpss_start_grp(VpssGrp);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "ss_mpi_vpss_start_grp failed with %#x", s32Ret);
        return s32Ret;
    }

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
        return TD_FAILURE;
    }

    ot_vi_vpss_mode_type otherViVpssMode;
    if(ViVpssMode == OT_VI_OFFLINE_VPSS_ONLINE) {
        otherViVpssMode = OT_VI_OFFLINE_VPSS_ONLINE;
    } else {
        otherViVpssMode = OT_VI_OFFLINE_VPSS_OFFLINE;
    }

    stVIVPSSMode.mode[0] = ViVpssMode;
    for (size_t i = 1; i < mode_count; ++i) {
        stVIVPSSMode.mode[i] = otherViVpssMode;
    }

    s32Ret = ss_mpi_sys_set_vi_vpss_mode(&stVIVPSSMode);
    if (s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ss_mpi_sys_set_vi_vpss_mode failed with %#x", s32Ret);
        return TD_FAILURE;
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
        LOGGER_ERROR(HIMPP, "Init VI err for %d!", s32Ret);
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

    stIspCtrlParam.stat_interval  = SENSOR_FRAME_RATE / 30;
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
    ext_data_type_t mipi_ext_data_type_default_attr;

    lane_divide_mode_t lane_divide_mode = LANE_DIVIDE_MODE_0;
    fd = open(MIPI_DEV_NODE, O_RDWR);
    if(fd < 0) {
        LOGGER_ERROR(HIMPP, "open %s failed!", MIPI_DEV_NODE);
        return TD_FAILURE;
    }

    // set mipi hs mode
    s32Ret = ioctl(fd, OT_MIPI_SET_HS_MODE, &lane_divide_mode);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_SET_HS_MODE failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }

    // enable mipi clock
    s32Ret = ioctl(fd, OT_MIPI_ENABLE_MIPI_CLOCK, &devno);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_ENABLE_MIPI_CLOCK failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }

    // reset mipi
    s32Ret = ioctl(fd, OT_MIPI_RESET_MIPI, &devno);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_RESET_MIPI failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }

    // enable sensor clock
    s32Ret = ioctl(fd, OT_MIPI_ENABLE_SENSOR_CLOCK, &snsDev);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_ENABLE_SENSOR_CLOCK failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }

    // reset sensor
    s32Ret = ioctl(fd, OT_MIPI_RESET_SENSOR, &snsDev);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_RESET_SENSOR failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }
    
    // set mipi dev attr (SC4336P)
    memset(&stcomboDevAttr, 0, sizeof(stcomboDevAttr));
    stcomboDevAttr.devno = devno;
    stcomboDevAttr.input_mode = INPUT_MODE_MIPI;
    stcomboDevAttr.data_rate = MIPI_DATA_RATE_X1;
    stcomboDevAttr.img_rect = {0, 0, SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT};
    stcomboDevAttr.mipi_attr = {
        DATA_TYPE_RAW_12BIT,
        OT_MIPI_WDR_MODE_NONE,
        {0, 2, -1, -1}
    };
    s32Ret = ioctl(fd, OT_MIPI_SET_DEV_ATTR, &stcomboDevAttr);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_SET_DEV_ATTR failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }

    memset(&mipi_ext_data_type_default_attr, 0, sizeof(ext_data_type_t));
    mipi_ext_data_type_default_attr = {
        .devno = 0,
        .num = 3,
        .ext_data_bit_width = {12, 12, 12},
        .ext_data_type = {0x2c, 0x2c, 0x2c}
    };
    s32Ret = ioctl(fd, OT_MIPI_SET_EXT_DATA_TYPE, &mipi_ext_data_type_default_attr);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_SET_EXT_DATA_TYPE failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }

    // unreset mipi
    s32Ret = ioctl(fd, OT_MIPI_UNRESET_MIPI, &devno);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_UNRESET_MIPI failed with %d, %s", errno, strerror(errno));
        close(fd);
        return TD_FAILURE;
    }

    // unreset sensor
    s32Ret = ioctl(fd, OT_MIPI_UNRESET_SENSOR, &snsDev);
    if(s32Ret != TD_SUCCESS) {
        LOGGER_ERROR(HIMPP, "ioctl OT_MIPI_UNRESET_SENSOR failed with %d, %s", errno, strerror(errno));
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
        .component_mask = {0xfff00000, 0x00000000},
        .scan_mode = OT_VI_SCAN_PROGRESSIVE,
        .ad_chn_id = { -1, -1, -1, -1 },
        .data_seq = OT_VI_DATA_SEQ_YVYU,
        .sync_cfg = {
            .vsync = OT_VI_VSYNC_FIELD,
            .vsync_neg = OT_VI_VSYNC_NEG_HIGH,
            .hsync = OT_VI_HSYNC_VALID_SIG,
            .hsync_neg = OT_VI_HSYNC_NEG_HIGH,
            .vsync_valid = OT_VI_VSYNC_VALID_SIG,
            .vsync_valid_neg = OT_VI_VSYNC_VALID_NEG_HIGH,
            .timing_blank = {0, 0, 0, 0, 0, 0, 0, 0, 0}
        },
        .data_type = OT_VI_DATA_TYPE_RAW,
        .data_reverse = TD_FALSE,
        .in_size = {SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT},
        .data_rate = OT_DATA_RATE_X1
    };
    // ot_vi_bind_pipe  stDevBindPipe = {};
    ot_vi_pipe_attr  stPipeAttr = {
        .pipe_bypass_mode = OT_VI_PIPE_BYPASS_NONE,
        .isp_bypass = TD_FALSE,
        .size = {SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT},
        .pixel_format = OT_PIXEL_FORMAT_RGB_BAYER_12BPP,
        // .compress_mode = OT_COMPRESS_MODE_LINE,
        .compress_mode = OT_COMPRESS_MODE_NONE,
        .frame_rate_ctrl = {-1, -1}
    };

    ot_vi_chn_attr stChnAttr = {
        .size = {SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT},
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

    // sample_comm_vi_set_grp_info

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
    
    s32Ret = ss_mpi_vi_enable_chn(ViPipe, ViChn);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "vi enable chn failed with %#x!", s32Ret);
        return s32Ret;
    }

    ss_mpi_vi_get_pipe_3dnr_attr(ViPipe, &dnr_attr);
    
    dnr_attr.enable = TD_TRUE;
    dnr_attr.compress_mode = OT_COMPRESS_MODE_FRAME;
    dnr_attr.nr_type = OT_NR_TYPE_VIDEO_NORM;
    dnr_attr.nr_motion_mode = OT_NR_MOTION_MODE_NORM;
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
        {0, 0, SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT},
        { SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT },
        30,
        OT_ISP_BAYER_BGGR,
        OT_WDR_MODE_NONE,
        0, TD_FALSE, TD_FALSE,
        {TD_FALSE, {0, 0, SENSOR_MAX_WIDTH, SENSOR_MAX_HEIGHT}}
    };

    ot_isp_sns_commbus uSnsBusInfo = {};
    uSnsBusInfo.i2c_dev = 0;

    // register sensor
    ot_isp_sns_obj* pstSnsObj = sc4336p_get_obj();
    if( pstSnsObj == TD_NULL )
    {
        LOGGER_ERROR(HIMPP, "get sc4336p sensor obj failed!");
        return TD_FAILURE;
    }

    if( pstSnsObj->pfn_register_callback == TD_NULL )
    {
        LOGGER_ERROR(HIMPP, "pfn_register_callback is null!");
        return TD_FAILURE;
    }
    pstSnsObj->pfn_register_callback(ViPipe, &stAeLib, &stAwbLib);

    // set_bus_info
    if( pstSnsObj->pfn_set_bus_info == TD_NULL )
    {
        LOGGER_ERROR(HIMPP, "pfn_set_bus_info is null!");
        return TD_FAILURE;
    }
    pstSnsObj->pfn_set_bus_info(ViPipe, uSnsBusInfo);

    // ae register
    s32Ret = ss_mpi_ae_register(ViPipe, &stAeLib);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "ss_mpi_ae_register failed with %#x!", s32Ret);
        return TD_FAILURE;
    }

    // awb_register
    s32Ret = ss_mpi_awb_register(ViPipe, &stAwbLib);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "ss_mpi_ae_register failed with %#x!", s32Ret);
        return TD_FAILURE;
    }
    
    s32Ret = ss_mpi_isp_mem_init(ViPipe);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"Init Ext memory failed with %#x!", s32Ret);
        return TD_FAILURE;
    }

    s32Ret = ss_mpi_isp_set_pub_attr(ViPipe, &stPubAttr);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"SetPubAttr failed with %#x!", s32Ret);
        return TD_FAILURE;
    }

    s32Ret = ss_mpi_isp_init(ViPipe);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"ss_mpi_isp_init failed with %#x!", s32Ret);
        return TD_FAILURE;
    }
    
    s32Ret = comm_isp_run(ViPipe);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP,"comm_isp_run failed with %#x!", s32Ret);
        return TD_FAILURE;
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

td_s32 videoProcessHi::enable_vpss_chn(ot_vpss_grp VpssGrp, ot_vpss_chn VpssChn, ot_vpss_chn_attr *pstVpssChnAttr, 
                        ot_vpss_ext_chn_attr *pstVpssExtChnAttr)
{
    td_s32 s32Ret = TD_FAILURE;

    if (VpssGrp < 0 || VpssGrp > OT_VPSS_MAX_GRP_NUM)
    {
        LOGGER_ERROR(HIMPP, "VpssGrp%d is out of rang[0,%d]", VpssGrp, OT_VPSS_MAX_GRP_NUM);
        return s32Ret;
    }

    if (VpssChn < 0 || VpssChn > OT_VPSS_MAX_CHN_NUM)
    {
        LOGGER_ERROR(HIMPP, "VpssChn%d is out of rang[0,%d]", VpssChn, OT_VPSS_MAX_CHN_NUM);
        return s32Ret;
    }

    if (TD_NULL == pstVpssChnAttr && TD_NULL == pstVpssExtChnAttr)
    {
        LOGGER_ERROR(HIMPP, "null ptr");
        return s32Ret;
    }

    if (VpssChn < OT_VPSS_MAX_PHYS_CHN_NUM)
    {
        s32Ret = ss_mpi_vpss_set_chn_attr(VpssGrp, VpssChn, pstVpssChnAttr);
        if (s32Ret != TD_SUCCESS)
        {
            LOGGER_ERROR(HIMPP, "mpi vpss set chn_attr failed, VpssChn %d VpssChn %d ret %#x", VpssGrp, VpssChn, s32Ret);
            return s32Ret;
        }
    }
    else
    {
        s32Ret = ss_mpi_vpss_set_ext_chn_attr(VpssGrp, VpssChn, pstVpssExtChnAttr);
        if (s32Ret != TD_SUCCESS)
        {
            LOGGER_ERROR(HIMPP, "mpi vpss set ext_chn_attr failed, VpssChn %d VpssChn %d ret %#x", VpssGrp, VpssChn, s32Ret);
            return s32Ret;
        }
    }

    if (VpssChn < OT_VPSS_MAX_PHYS_CHN_NUM)
    {
        if(rotateBSupport)
        {
            ot_rotation_attr rotate_attr = { };
            rotate_attr.enable = TD_TRUE;
            rotate_attr.rotation_type = OT_ROTATION_ANG_FIXED;
            rotate_attr.rotation_fixed = OT_ROTATION_90;
            s32Ret = ss_mpi_vpss_set_chn_rotation(VpssGrp, VpssChn, &rotate_attr);
            if(s32Ret != TD_SUCCESS)
            {
                LOGGER_ERROR(HIMPP, "mpi vpss set chn_rotation failed, VpssGrp %d VpssChn %d ret %#x", VpssGrp, VpssChn, s32Ret);
                return s32Ret;
            }
        }
    }

    if (wrap_enable)	// 开卷绕
    {
        if (VpssChn == 0)   // vpss limit! just vpss chan0 support wrap
        {
            s32Ret = ss_mpi_vpss_set_chn_buf_wrap(VpssGrp, VpssChn, &stVpssChnBufWrap);
            if (s32Ret != TD_SUCCESS)
            {
                LOGGER_ERROR(HIMPP, "mpi vpss set chn_buf_wrap failed, VpssGrp %d VpssChn %d ret %#x", VpssGrp, VpssChn, s32Ret);
                return s32Ret;
            }
        }
    }

    s32Ret = ss_mpi_vpss_enable_chn(VpssGrp, VpssChn);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "mpi vpss enable chn failed, VpssGrp %d VpssChn %d ret %#x", VpssGrp, VpssChn, s32Ret);
        return s32Ret;
    }

    return TD_SUCCESS;
}

td_s32 videoProcessHi::bind_vi_vpss(ot_vi_pipe ViPipe, ot_vi_chn ViChn, 
                           ot_vpss_grp VpssGrp, ot_vpss_chn VpssChn)
{
    ot_mpp_chn stSrcChn;
    ot_mpp_chn stDestChn;

    stSrcChn.mod_id   = OT_ID_VI;
    stSrcChn.dev_id  = ViPipe;
    stSrcChn.chn_id  = ViChn;

    stDestChn.mod_id  = OT_ID_VPSS;
    stDestChn.dev_id = VpssGrp;
    stDestChn.chn_id = VpssChn;

    int s32Ret = ss_mpi_sys_bind(&stSrcChn, &stDestChn);
    if(s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "sys bind failed, ViPipe %d, ViChn %d, VpssGrp %d, VpssChn %d, ret %#x!", 
            ViPipe, ViChn, VpssGrp, VpssChn, s32Ret);
	    td_bool abChnEnable[OT_VPSS_MAX_PHYS_CHN_NUM] = {TD_TRUE, TD_TRUE, TD_FALSE};
		vpss_stop(VpssGrp, abChnEnable);
        return s32Ret;
    }

    return s32Ret;
}

td_s32 videoProcessHi::vpss_stop(ot_vpss_grp VpssGrp, td_bool* pabChnEnable)
{
    td_s32 j;
    td_s32 s32Ret = TD_SUCCESS;
    ot_vpss_chn VpssChn;

    for (j = 0; j < OT_VPSS_MAX_PHYS_CHN_NUM; j++)
    {
        if(TD_TRUE == pabChnEnable[j])
        {
            VpssChn = j;
            s32Ret = ss_mpi_vpss_disable_chn(VpssGrp, VpssChn);

            if (s32Ret != TD_SUCCESS)
            {
                LOGGER_ERROR(HIMPP, "failed with %#x!", s32Ret);
                return s32Ret;
            }
        }
    }

    s32Ret = ss_mpi_vpss_stop_grp(VpssGrp);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "failed with %#x!", s32Ret);
        return s32Ret;
    }

    s32Ret = ss_mpi_vpss_destroy_grp(VpssGrp);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "failed with %#x!", s32Ret);
        return s32Ret;
    }

    return s32Ret;
}

}   // hiMppMedia