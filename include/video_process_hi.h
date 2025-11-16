#ifndef VIDEO_PROCESS_HI_H
#define VIDEO_PROCESS_HI_H

#include <ot_common_vpss.h>
#include <ot_type.h>
#include <thread>

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

    int hi_mpp_sys_init();

    int hi_mpp_vi_init();

    td_void comm_vpss_get_default_chn_attr(ot_vpss_chn_attr *chn_attr);

    td_s32 comm_vpss_get_wrap_cfg(ot_vpss_chn_attr chn_attr[OT_VPSS_MAX_PHYS_CHN_NUM],
    ot_vi_vpss_mode_type mode, td_u32 full_lines_std, ot_vpss_chn_buf_wrap_attr *wrap_attr);

    td_s32 set_vi_vpss_mode(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn, 
    ot_vi_vpss_mode_type ViVpssMode);

    td_s32 init_vi_module(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    td_s32 init_vi_process(ot_vi_vpss_mode_type ViVpssMode, ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    td_s32 comm_vi_start_vi(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    td_s32 comm_vi_start_mipi();

    td_s32 comm_vi_create_vi(ot_vi_dev ViDev, ot_vi_pipe ViPipe, ot_vi_chn ViChn);

    td_s32 comm_vi_create_isp(ot_vi_pipe ViPipe);

    td_s32 comm_isp_run(ot_isp_dev IspDev);

    td_s32 comm_isp_task(ot_isp_dev IspDev);

private:

    static constexpr const int VIDEO_STRETCH_WIDTH = 0;
    static constexpr const int VIDEO_STRETCH_HEIGHT = 0;
    static constexpr const int VI_WIDTH = 1920;
    static constexpr const int VI_HEIGHT = 1080;
	static constexpr const int IVP_SMD_W = 640;
	static constexpr const int IVP_SMD_H = 384;
    static constexpr const int SENSOR_MAX_WIDTH = 2560;
    static constexpr const int SENSOR_MAX_HRIGHT = 1440;
    static constexpr const int SENSOR_FRAME_RATE = 30;
    static constexpr const char* MIPI_DEV_NODE = "/dev/ot_mipi_rx";

    bool wrap_enable;   // 低延时卷绕模式，300w和400w分辨率时使用
    bool video_stretch_enable;  // 200W拉伸为300W
    ot_vpss_chn_buf_wrap_attr stVpssChnBufWrap;
    std::thread ispThread;

};

#endif  // VIDEO_PROCESS_HI_H