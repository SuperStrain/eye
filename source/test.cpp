#include "test.h"
#include <ot_common.h>
#include <ot_common_vpss.h>
#include "global_constants.h"
#include <ss_mpi_vpss.h>
#include "logger.h"
#include <pthread.h>
#include <thread>
#include <unistd.h>
#include <ss_mpi_sys_mem.h>

ot_vpss_grp g_vpssGrp = 0;
ot_vpss_chn g_vpssChn = 1;

static td_s32 SAMPLE_COMM_VPSS_EnableChn_Md(ot_vpss_grp VpssGrp, ot_vpss_chn VpssChn, 
                                                  ot_vpss_chn_attr *pstVpssChnAttr,
                                                  ot_vpss_ext_chn_attr *pstVpssExtChnAttr)
{

    td_s32 s32Ret;

    if (VpssGrp < 0 || VpssGrp > OT_VPSS_MAX_GRP_NUM)
    {
        LOGGER_ERROR(HIMPP, "VpssGrp%d is out of rang[0,%d].", VpssGrp, OT_VPSS_MAX_GRP_NUM);
        return TD_FAILURE;
    }

    if (VpssChn < 0 || VpssChn > OT_VPSS_MAX_CHN_NUM)
    {
        LOGGER_ERROR(HIMPP, "VpssChn%d is out of rang[0,%d]. ", VpssChn, OT_VPSS_MAX_CHN_NUM);
        return TD_FAILURE;
    }

    if (TD_NULL == pstVpssChnAttr && TD_NULL == pstVpssExtChnAttr)
    {
        LOGGER_ERROR(HIMPP, "null ptr,line%d.", __LINE__);
        return TD_FAILURE;
    }

    if (VpssChn < OT_VPSS_MAX_PHYS_CHN_NUM)
    {
        s32Ret = ss_mpi_vpss_set_chn_attr(VpssGrp, VpssChn, pstVpssChnAttr);
        if (s32Ret != TD_SUCCESS)
        {
            LOGGER_ERROR(HIMPP, "ss_mpi_vpss_set_chn_attr failed with %#x", s32Ret);
            return TD_FAILURE;
        }
    }
    else
    {
        if(TD_NULL != pstVpssExtChnAttr)
        {
                LOGGER_NOTICE(HIMPP, "=========================VpssChn:%d", VpssChn);
                s32Ret = ss_mpi_vpss_set_ext_chn_attr(VpssGrp, VpssChn, pstVpssExtChnAttr);
                if (s32Ret != TD_SUCCESS)
                {
                    LOGGER_ERROR(HIMPP, "failed with %#x", s32Ret);
                    return TD_FAILURE;
                }
        }
    }

    if (VpssChn < OT_VPSS_MAX_PHYS_CHN_NUM)
    {
        ot_rotation_attr rotate_attr = { };
        rotate_attr.enable = TD_TRUE;
        rotate_attr.rotation_type = OT_ROTATION_ANG_FIXED;
        rotate_attr.rotation_fixed = OT_ROTATION_0;
        s32Ret = ss_mpi_vpss_set_chn_rotation(VpssGrp, VpssChn, &rotate_attr);
        if(s32Ret != TD_SUCCESS)
        {
            LOGGER_ERROR(HIMPP, "HI_MPI_VPSS_SetRotate failed with %#x!", s32Ret);
            return s32Ret;
        }
    }

    s32Ret = ss_mpi_vpss_enable_chn(VpssGrp, VpssChn);
    if (s32Ret != TD_SUCCESS)
    {
        LOGGER_ERROR(HIMPP, "ss_mpi_vpss_enable_chn failed with %#x", s32Ret);
        return TD_FAILURE;
    }

    return TD_SUCCESS;
}

static void yuv_get_frame_process()
{
    pthread_setname_np(pthread_self(), "yuv_get_frame_process");
    ot_video_frame_info stFrmInfo = { };
    td_s32 s32GetFrameMilliSec = 2000;
    td_s32 s32Ret = TD_SUCCESS;
    while(1) {
        s32Ret = ss_mpi_vpss_get_chn_frame(g_vpssGrp, g_vpssChn, &stFrmInfo, s32GetFrameMilliSec);
        if (TD_SUCCESS != s32Ret)
        {
            LOGGER_ERROR(HIMPP, "ss_mpi_vpss_get_chn_frame chn(%d) fail,Error(%#x)", g_vpssChn, s32Ret);
            usleep(50*1000);
            continue;
        }

        td_u32 mmapSize = stFrmInfo.video_frame.stride[0] * stFrmInfo.video_frame.height * 3 / 2;
        LOGGER_NOTICE(HIMPP, "mmapSize=%u, stride0=%u, stride1=%u, height=%u, width=%u", 
            mmapSize, stFrmInfo.video_frame.stride[0], stFrmInfo.video_frame.stride[1], stFrmInfo.video_frame.height, stFrmInfo.video_frame.width);
        td_void * vir_addr = ss_mpi_sys_mmap(stFrmInfo.video_frame.phys_addr[0], mmapSize);
        if(!vir_addr) {
            LOGGER_ERROR(HIMPP, "vir_addr is null");
            s32Ret = ss_mpi_vpss_release_chn_frame(g_vpssGrp, g_vpssChn, &stFrmInfo);
            if (TD_SUCCESS != s32Ret)
            {
                LOGGER_ERROR(HIMPP, "ss_mpi_vpss_release_chn_frame fail, chn(%d),Error(%#x)", g_vpssGrp, s32Ret);
            }
            usleep(1 * 1000 * 1000);
            continue;            
        }

        // 消费帧 写
        // LOGGER_NOTICE(HIMPP, "Get vpss chn %d frame success, width %d, height %d, pts %llu", 
        //     g_vpssChn, stFrmInfo.video_frame.width, stFrmInfo.video_frame.height, stFrmInfo.video_frame.pts);
        static int i = 0;
        std::string filename = "/run/nfs/yuvdata/yuv_frame_" + std::to_string(i) + ".yuv";
        FILE* fp = fopen(filename.c_str(), "wb");
        if (fp) {
            // Y
            for(int i = 0; i < (int)stFrmInfo.video_frame.height; i++) {
                fwrite((td_u8*)vir_addr + i * stFrmInfo.video_frame.stride[0], 1, stFrmInfo.video_frame.width, fp);
            }
            // UV
            td_u8* uv_addr = (td_u8*)vir_addr + stFrmInfo.video_frame.stride[0] * stFrmInfo.video_frame.height;
            for(int i = 0; i < (int)(stFrmInfo.video_frame.height / 2); i++) {
                fwrite(uv_addr + i * stFrmInfo.video_frame.stride[1], 1, stFrmInfo.video_frame.width, fp);
            }
            fclose(fp);
        } else {
            LOGGER_ERROR(HIMPP, "fopen %s failed!", filename.c_str());
        }
        i++;

        ss_mpi_sys_munmap(vir_addr, mmapSize);
        s32Ret = ss_mpi_vpss_release_chn_frame(g_vpssGrp, g_vpssChn, &stFrmInfo);
        if (TD_SUCCESS != s32Ret)
        {
            LOGGER_ERROR(HIMPP, "ss_mpi_vpss_release_chn_frame fail, chn(%d),Error(%#x)", g_vpssGrp, s32Ret);
        }

    }
}

/* yuv取帧功能测试 */
static int yuv_frame_test()
{
    /* start */
    // td_s32 s32Ret = TD_SUCCESS;
    // ot_vpss_ext_chn_attr stVpssExtChnAttr = {};
    // stVpssExtChnAttr.bind_chn = 1;
    // stVpssExtChnAttr.src_type = OT_EXT_CHN_SRC_TYPE_TAIL;
    // stVpssExtChnAttr.frame_rate.src_frame_rate = hiMppMedia::maxFrameRate;
    // stVpssExtChnAttr.frame_rate.dst_frame_rate = stVpssExtChnAttr.frame_rate.src_frame_rate;
    // stVpssExtChnAttr.pixel_format = OT_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    // stVpssExtChnAttr.width = hiMppMedia::maxWidth;
    // stVpssExtChnAttr.height = hiMppMedia::maxHeight;
    // stVpssExtChnAttr.compress_mode = OT_COMPRESS_MODE_NONE;
    // stVpssExtChnAttr.video_format = OT_VIDEO_FORMAT_LINEAR;
    // stVpssExtChnAttr.dynamic_range = OT_DYNAMIC_RANGE_SDR8;
    // stVpssExtChnAttr.depth = 1;

    // s32Ret = SAMPLE_COMM_VPSS_EnableChn_Md(g_vpssGrp, g_vpssChn, NULL, &stVpssExtChnAttr);
    // if (TD_SUCCESS != s32Ret)
    // {
    //     LOGGER_ERROR(HIMPP, "Enable vpss chn %d failed!", g_vpssChn);
    //     return TD_FAILURE;
    // }
    LOGGER_NOTICE(HIMPP, "Enable vpss chn %d success!", g_vpssChn);

    std::thread get_yuv_thread = std::thread(yuv_get_frame_process);
    get_yuv_thread.detach();

    return 0;
}

int test_main()
{
    yuv_frame_test();
    return 0;
}