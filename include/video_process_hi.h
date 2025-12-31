#ifndef VIDEO_PROCESS_HI_H
#define VIDEO_PROCESS_HI_H

#include <ot_common_vpss.h>
#include <ot_type.h>
#include <thread>
#include "global_constants.h"

namespace hiMppMedia {
class videoProcessHi
{
public:

    videoProcessHi(const videoProcessHi&) = delete;

    videoProcessHi &operator=(const videoProcessHi &) = delete;

    static videoProcessHi &getInstance()
    {
        static videoProcessHi instance;
        return instance;
    }

    int init();

private:

    videoProcessHi();
    
    ~videoProcessHi();

    /* system module */
    td_s32 hi_mpp_sys_init();

    /* vi module */
    td_s32 hi_mpp_vi_init(ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    td_s32 init_vi_module(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    td_s32 init_vi_process(ot_vi_vpss_mode_type ViVpssMode, ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    td_s32 comm_vi_start_vi(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    /* mipi */
    td_s32 comm_vi_start_mipi();

    td_s32 comm_vi_create_vi(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    /* isp */
    td_s32 comm_vi_create_isp(ot_vi_pipe ViPipe);

    td_s32 comm_isp_run(ot_isp_dev IspDev);

    td_s32 comm_isp_task(ot_isp_dev IspDev);

    /* vpss module */
    td_s32 set_vi_vpss_mode(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn, 
    ot_vi_vpss_mode_type ViVpssMode);

    td_s32 init_vpss_module(ot_vpss_grp VpssGrp, td_bool *abChnEnable);

    td_void comm_vpss_get_default_chn_attr(ot_vpss_chn_attr *chn_attr);

    td_s32 comm_vpss_get_wrap_cfg(ot_vpss_chn_attr chn_attr[OT_VPSS_MAX_PHYS_CHN_NUM],
    ot_vi_vpss_mode_type mode, td_u32 full_lines_std, ot_vpss_chn_buf_wrap_attr *wrap_attr);

    td_s32 enable_vpss_chn(ot_vpss_grp VpssGrp, ot_vpss_chn VpssChn, ot_vpss_chn_attr *pstVpssChnAttr, 
                           ot_vpss_ext_chn_attr *pstVpssExtChnAttr);
    
    td_s32 bind_vi_vpss(ot_vi_pipe ViPipe, ot_vi_chn ViChn, 
                           ot_vpss_grp VpssGrp, ot_vpss_chn VpssChn);

    td_s32 vpss_stop(ot_vpss_grp VpssGrp, td_bool* pabChnEnable);

    /* venc module */

private:

    static constexpr int VIDEO_STRETCH_WIDTH = 0;
    static constexpr int VIDEO_STRETCH_HEIGHT = 0;
    static constexpr int VI_WIDTH0 = SENSOR_MAX_WIDTH;
    static constexpr int VI_HEIGHT0 = SENSOR_MAX_HEIGHT;
    static constexpr int VI_WIDTH1 = 720;
    static constexpr int VI_HEIGHT1 = 480;
	static constexpr int VI_WIDTH2 = 640;
	static constexpr int VI_HEIGHT2 = 384;
    static constexpr int SENSOR_FRAME_RATE = maxFrameRate;
    static constexpr const char* MIPI_DEV_NODE = "/dev/ot_mipi_rx";
    static constexpr bool rotateBSupport = false;

    bool wrap_enable;   // 低延时卷绕模式，300w和400w分辨率时使用
    bool video_stretch_enable;  // 200W拉伸为300W
    ot_vpss_chn_buf_wrap_attr stVpssChnBufWrap;
    std::thread ispThread;

};

} // namespace hiMppMedia

#endif  // VIDEO_PROCESS_HI_H