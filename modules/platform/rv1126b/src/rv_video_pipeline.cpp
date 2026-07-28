#include "rv_video_pipeline.h"

#include <rk_comm_venc.h>
#include <rk_comm_vi.h>
#include <rk_mpi_sys.h>
#include <rk_mpi_venc.h>
#include <rk_mpi_vi.h>

#include <cstring>

#include "logger.h"

namespace rv1126bMedia {

namespace {

constexpr int kDevId = 0;
constexpr int kPipeId = 0;
constexpr int kViMainChn = 3;
constexpr int kViSubChn = 2;
constexpr int kVencMainChn = 0;
constexpr int kVencSubChn = 1;
constexpr int kVencMjpegChn = 2;
// 视频参数对应 IMX415 mode[0]：sensor native 3864x2192@30fps 10bit Linear（NO_HDR），
// 由 rockit 默认协商，无需应用层显式设置（与官方 test_mpi_vi.cpp 流程一致）。
// 主码流 stSize = sensor native，ISP 无缩放；子码流/MJPEG 由 ISP 下采样。
// 详见 atk_dlrv1126b_linux6.1_sdk/kernel-6.1/drivers/media/i2c/imx415.c supported_modes[0]。
constexpr int kMainWidth = 3864;
constexpr int kMainHeight = 2192;
constexpr int kSubWidth = 1280;
constexpr int kSubHeight = 720;
constexpr int kMjpegWidth = 1280;
constexpr int kMjpegHeight = 720;
constexpr int kFrameRate = 30;
constexpr int kMainBitrate = 8192;
constexpr int kSubBitrate = 2048;
constexpr int kMjpegBitrate = 2048;

} // namespace

RvVideoPipeline& RvVideoPipeline::getInstance() {
    static RvVideoPipeline instance;
    return instance;
}

RvVideoPipeline::RvVideoPipeline() : initialized_(false) {}
RvVideoPipeline::~RvVideoPipeline() {}

int RvVideoPipeline::init() {
    if (initialized_) return 0;

    int ret = init_sys();
    if (ret != 0) return ret;
    ret = init_vi_dev();
    if (ret != 0) goto fail_sys;
    ret = init_vi_channels();
    if (ret != 0) goto fail_vi_dev;
    ret = init_venc_channels();
    if (ret != 0) goto fail_venc;
    ret = bind_channels();
    if (ret != 0) goto fail_bind;

    initialized_ = true;
    LOGGER_INFO(ROCKIT, "RV1126B video pipeline initialized");
    return 0;

fail_bind:
    unbind_channels();
fail_venc:
    deinit_venc_channels();
fail_vi_channels:
    deinit_vi_channels();
fail_vi_dev:
    deinit_vi_dev();
fail_sys:
    RK_MPI_SYS_Exit();
    return ret;
}

int RvVideoPipeline::deinit() {
    if (!initialized_) return 0;
    unbind_channels();
    deinit_venc_channels();
    deinit_vi_channels();
    deinit_vi_dev();
    RK_MPI_SYS_Exit();
    initialized_ = false;
    return 0;
}

int RvVideoPipeline::start() { return 0; }
int RvVideoPipeline::stop() { return 0; }
int RvVideoPipeline::createChannel(int, CodecType, Size) { return 0; }
int RvVideoPipeline::destroyChannel(int) { return 0; }
int RvVideoPipeline::startChannel(int) { return 0; }
int RvVideoPipeline::stopChannel(int) { return 0; }

int RvVideoPipeline::init_sys() {
    int ret = RK_MPI_SYS_Init();
    if (ret != RK_SUCCESS) {
        LOGGER_ERROR(ROCKIT, "RK_MPI_SYS_Init failed: %#x", ret);
        return ret;
    }
    return 0;
}

int RvVideoPipeline::init_vi_dev() {
    VI_DEV_ATTR_S dev_attr;
    VI_DEV_BIND_PIPE_S bind_pipe;
    std::memset(&dev_attr, 0, sizeof(dev_attr));
    std::memset(&bind_pipe, 0, sizeof(bind_pipe));

    int ret = RK_MPI_VI_GetDevAttr(kDevId, &dev_attr);
    if (ret == RK_ERR_VI_NOT_CONFIG) {
        ret = RK_MPI_VI_SetDevAttr(kDevId, &dev_attr);
        if (ret != RK_SUCCESS) {
            LOGGER_ERROR(ROCKIT, "RK_MPI_VI_SetDevAttr failed: %#x", ret);
            return ret;
        }
    } else if (ret != RK_SUCCESS) {
        LOGGER_ERROR(ROCKIT, "RK_MPI_VI_GetDevAttr failed: %#x", ret);
        return ret;
    }

    ret = RK_MPI_VI_GetDevIsEnable(kDevId);
    if (ret != RK_SUCCESS) {
        ret = RK_MPI_VI_EnableDev(kDevId);
        if (ret != RK_SUCCESS) {
            LOGGER_ERROR(ROCKIT, "RK_MPI_VI_EnableDev failed: %#x", ret);
            return ret;
        }
        bind_pipe.u32Num = 1;
        bind_pipe.PipeId[0] = kPipeId;
        ret = RK_MPI_VI_SetDevBindPipe(kDevId, &bind_pipe);
        if (ret != RK_SUCCESS) {
            LOGGER_ERROR(ROCKIT, "RK_MPI_VI_SetDevBindPipe failed: %#x", ret);
            return ret;
        }
    }
    return 0;
}

static int set_vi_channel(int chn, int width, int height, int max_width, int max_height) {
    VI_CHN_ATTR_S attr;
    std::memset(&attr, 0, sizeof(attr));
    attr.stIspOpt.u32BufCount = 3;
    attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    attr.stIspOpt.stMaxSize.u32Width = max_width;
    attr.stIspOpt.stMaxSize.u32Height = max_height;
    attr.stSize.u32Width = width;
    attr.stSize.u32Height = height;
    attr.u32Depth = 0;
    attr.enPixelFormat = RK_FMT_YUV420SP;
    attr.enCompressMode = COMPRESS_MODE_NONE;

    int ret = RK_MPI_VI_SetChnAttr(kPipeId, chn, &attr);
    if (ret != RK_SUCCESS) return ret;
    return RK_MPI_VI_EnableChn(kPipeId, chn);
}

int RvVideoPipeline::init_vi_channels() {
    int ret = set_vi_channel(kViMainChn, kMainWidth, kMainHeight, kMainWidth, kMainHeight);
    if (ret != RK_SUCCESS) {
        LOGGER_ERROR(ROCKIT, "main VI channel init failed: %#x", ret);
        return ret;
    }
    ret = set_vi_channel(kViSubChn, kSubWidth, kSubHeight, kSubWidth, kSubHeight);
    if (ret != RK_SUCCESS) {
        LOGGER_ERROR(ROCKIT, "sub VI channel init failed: %#x", ret);
        return ret;
    }
    return 0;
}

static int create_venc_channel(int chn, CodecType codec, int width, int height, int bitrate) {
    VENC_CHN_ATTR_S attr;
    std::memset(&attr, 0, sizeof(attr));

    attr.stVencAttr.u32MaxPicWidth = width;
    attr.stVencAttr.u32MaxPicHeight = height;
    attr.stVencAttr.u32PicWidth = width;
    attr.stVencAttr.u32PicHeight = height;
    attr.stVencAttr.u32VirWidth = width;
    attr.stVencAttr.u32VirHeight = height;
    attr.stVencAttr.u32StreamBufCnt = 4;
    attr.stVencAttr.u32BufSize = width * height;

    if (codec == CodecType::H265) {
        attr.stVencAttr.enType = RK_VIDEO_ID_HEVC;
        attr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
        attr.stRcAttr.stH265Cbr.u32BitRate = bitrate;
        attr.stRcAttr.stH265Cbr.u32Gop = kFrameRate * 2;
        attr.stRcAttr.stH265Cbr.fr32DstFrameRateDen = 1;
        attr.stRcAttr.stH265Cbr.fr32DstFrameRateNum = kFrameRate;
        attr.stRcAttr.stH265Cbr.u32SrcFrameRateDen = 1;
        attr.stRcAttr.stH265Cbr.u32SrcFrameRateNum = kFrameRate;
    } else if (codec == CodecType::H264) {
        attr.stVencAttr.enType = RK_VIDEO_ID_AVC;
        attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
        attr.stRcAttr.stH264Cbr.u32BitRate = bitrate;
        attr.stRcAttr.stH264Cbr.u32Gop = kFrameRate * 2;
        attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
        attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = kFrameRate;
        attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
        attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = kFrameRate;
    } else {
        attr.stVencAttr.enType = RK_VIDEO_ID_MJPEG;
        attr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
        attr.stRcAttr.stMjpegCbr.u32BitRate = bitrate;
        attr.stRcAttr.stMjpegCbr.fr32DstFrameRateDen = 1;
        attr.stRcAttr.stMjpegCbr.fr32DstFrameRateNum = 10;
        attr.stRcAttr.stMjpegCbr.u32SrcFrameRateDen = 1;
        attr.stRcAttr.stMjpegCbr.u32SrcFrameRateNum = kFrameRate;
    }

    return RK_MPI_VENC_CreateChn(chn, &attr);
}

static int start_venc_recv(int chn) {
    VENC_RECV_PIC_PARAM_S recv_param;
    std::memset(&recv_param, 0, sizeof(recv_param));
    recv_param.s32RecvPicNum = -1;
    return RK_MPI_VENC_StartRecvFrame(chn, &recv_param);
}

int RvVideoPipeline::init_venc_channels() {
    int ret = create_venc_channel(kVencMainChn, CodecType::H265, kMainWidth, kMainHeight, kMainBitrate);
    if (ret != RK_SUCCESS) return ret;
    venc_created_[kVencMainChn] = true;
    ret = start_venc_recv(kVencMainChn);
    if (ret != RK_SUCCESS) return ret;

    ret = create_venc_channel(kVencSubChn, CodecType::H264, kSubWidth, kSubHeight, kSubBitrate);
    if (ret != RK_SUCCESS) return ret;
    venc_created_[kVencSubChn] = true;
    ret = start_venc_recv(kVencSubChn);
    if (ret != RK_SUCCESS) return ret;

    ret = create_venc_channel(kVencMjpegChn, CodecType::MJPEG, kMjpegWidth, kMjpegHeight, kMjpegBitrate);
    if (ret != RK_SUCCESS) return ret;
    venc_created_[kVencMjpegChn] = true;
    ret = start_venc_recv(kVencMjpegChn);
    if (ret != RK_SUCCESS) return ret;

    return 0;
}

static int bind_vi_to_venc(int vi_chn_id, int venc_chn_id) {
    MPP_CHN_S src;
    MPP_CHN_S dst;
    std::memset(&src, 0, sizeof(src));
    std::memset(&dst, 0, sizeof(dst));
    src.enModId = RK_ID_VI;
    src.s32DevId = 0;
    src.s32ChnId = vi_chn_id;
    dst.enModId = RK_ID_VENC;
    dst.s32DevId = 0;
    dst.s32ChnId = venc_chn_id;
    return RK_MPI_SYS_Bind(&src, &dst);
}

static int unbind_vi_from_venc(int vi_chn_id, int venc_chn_id) {
    MPP_CHN_S src;
    MPP_CHN_S dst;
    std::memset(&src, 0, sizeof(src));
    std::memset(&dst, 0, sizeof(dst));
    src.enModId = RK_ID_VI;
    src.s32DevId = 0;
    src.s32ChnId = vi_chn_id;
    dst.enModId = RK_ID_VENC;
    dst.s32DevId = 0;
    dst.s32ChnId = venc_chn_id;
    return RK_MPI_SYS_UnBind(&src, &dst);
}

int RvVideoPipeline::bind_channels() {
    int ret = bind_vi_to_venc(kViMainChn, kVencMainChn);
    if (ret != RK_SUCCESS) return ret;
    ret = bind_vi_to_venc(kViSubChn, kVencSubChn);
    if (ret != RK_SUCCESS) return ret;
    return bind_vi_to_venc(kViSubChn, kVencMjpegChn);
}

int RvVideoPipeline::unbind_channels() {
    int ret, first_err = RK_SUCCESS;
    ret = unbind_vi_from_venc(kViMainChn, kVencMainChn);
    if (ret != RK_SUCCESS) {
        LOGGER_WARN(ROCKIT, "RK_MPI_SYS_UnBind(main) failed: %#x", ret);
        if (first_err == RK_SUCCESS) first_err = ret;
    }
    ret = unbind_vi_from_venc(kViSubChn, kVencSubChn);
    if (ret != RK_SUCCESS) {
        LOGGER_WARN(ROCKIT, "RK_MPI_SYS_UnBind(sub) failed: %#x", ret);
        if (first_err == RK_SUCCESS) first_err = ret;
    }
    ret = unbind_vi_from_venc(kViSubChn, kVencMjpegChn);
    if (ret != RK_SUCCESS) {
        LOGGER_WARN(ROCKIT, "RK_MPI_SYS_UnBind(mjpeg) failed: %#x", ret);
        if (first_err == RK_SUCCESS) first_err = ret;
    }
    return first_err;
}

int RvVideoPipeline::deinit_venc_channels() {
    int ret;
    if (venc_created_[kVencMainChn]) {
        ret = RK_MPI_VENC_StopRecvFrame(kVencMainChn);
        if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VENC_StopRecvFrame(main) failed: %#x", ret);
    }
    if (venc_created_[kVencSubChn]) {
        ret = RK_MPI_VENC_StopRecvFrame(kVencSubChn);
        if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VENC_StopRecvFrame(sub) failed: %#x", ret);
    }
    if (venc_created_[kVencMjpegChn]) {
        ret = RK_MPI_VENC_StopRecvFrame(kVencMjpegChn);
        if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VENC_StopRecvFrame(mjpeg) failed: %#x", ret);
    }
    if (venc_created_[kVencMainChn]) {
        ret = RK_MPI_VENC_DestroyChn(kVencMainChn);
        if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VENC_DestroyChn(main) failed: %#x", ret);
        venc_created_[kVencMainChn] = false;
    }
    if (venc_created_[kVencSubChn]) {
        ret = RK_MPI_VENC_DestroyChn(kVencSubChn);
        if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VENC_DestroyChn(sub) failed: %#x", ret);
        venc_created_[kVencSubChn] = false;
    }
    if (venc_created_[kVencMjpegChn]) {
        ret = RK_MPI_VENC_DestroyChn(kVencMjpegChn);
        if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VENC_DestroyChn(mjpeg) failed: %#x", ret);
        venc_created_[kVencMjpegChn] = false;
    }
    return 0;
}

int RvVideoPipeline::deinit_vi_channels() {
    int ret = RK_MPI_VI_DisableChn(kPipeId, kViMainChn);
    if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VI_DisableChn(main) failed: %#x", ret);
    ret = RK_MPI_VI_DisableChn(kPipeId, kViSubChn);
    if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VI_DisableChn(sub) failed: %#x", ret);
    return 0;
}

int RvVideoPipeline::deinit_vi_dev() {
    int ret = RK_MPI_VI_DisableDev(kDevId);
    if (ret != RK_SUCCESS) LOGGER_WARN(ROCKIT, "RK_MPI_VI_DisableDev failed: %#x", ret);
    return 0;
}

} // namespace rv1126bMedia
