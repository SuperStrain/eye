#ifndef VIDEO_VENC_HI_H
#define VIDEO_VENC_HI_H

#include <ot_type.h>
#include <ot_common_venc.h>
#include "global_constants.h"

namespace hiMppMedia {

class video_venc_hi
{
private:

public:
    video_venc_hi();
    ~video_venc_hi();

    /* H265 */
    td_void comm_venc_h265_abr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h265_cbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h265_fixqp_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 frame_rate);

    td_void comm_venc_h265_vbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h265_avbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h265_cvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate, ot_pic_size size);

    td_void comm_venc_h265_qvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h265_qpmap_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);
    
    /* H264 */
    td_void comm_venc_h264_abr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h264_cbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h264_fixqp_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 frame_rate);

    td_void comm_venc_h264_vbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h264_avbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h264_cvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate, ot_pic_size size);

    td_void comm_venc_h264_qvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_h264_qpmap_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time,
    td_u32 frame_rate);

    /* MJPEG */
    td_void comm_venc_mjpeg_fixqp_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 frame_rate);

    td_void comm_venc_mjpeg_cbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 stats_time,
    td_u32 frame_rate);

    td_void comm_venc_mjpeg_vbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 stats_time,
    td_u32 frame_rate);
    
    /* JPEG */
    td_s32 comm_venc_jpeg_param_init(ot_venc_chn_attr *venc_chn_attr);

    /* SVAC3 */
private:
    td_void comm_venc_set_h265_cvbr_bit_rate(ot_venc_chn_attr *venc_chn_attr, ot_venc_h265_cvbr *h265_cvbr,
    td_u32 frame_rate, ot_pic_size size);

    td_void comm_venc_set_h264_cvbr_bit_rate(ot_venc_chn_attr *venc_chn_attr, ot_venc_h264_cvbr *h264_cvbr,
        td_u32 frame_rate, ot_pic_size size);
};

}
#endif // VIDEO_VENC_HI_H
