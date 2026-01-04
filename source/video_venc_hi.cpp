#include "video_venc_hi.h"

namespace hiMppMedia {  

video_venc_hi::video_venc_hi()
{

}

video_venc_hi::~video_venc_hi()
{

}


td_void video_venc_hi::comm_venc_h265_abr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h265_abr h265_abr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_ABR;
    h265_abr.gop = gop;
    h265_abr.stats_time = stats_time;     /* stream rate statics time(s) */
    h265_abr.src_frame_rate = frame_rate; /* input (vi) frame rate */
    h265_abr.dst_frame_rate = frame_rate; /* target frame rate */
    h265_abr.vbv_buf_delay  = 50; // 50: vbv_buf_delay
    h265_abr.bit_rate = ((td_u64)3072 * (pic_width * pic_height)) / (1920 * 1080); /* 3072: 3M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h265_abr = h265_abr;
}

td_void video_venc_hi::comm_venc_h265_cbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h265_cbr h265_cbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_CBR;
    h265_cbr.gop = gop;
    h265_cbr.stats_time = stats_time;     /* stream rate statics time(s) */
    h265_cbr.src_frame_rate = frame_rate; /* input (vi) frame rate */
    h265_cbr.dst_frame_rate = frame_rate; /* target frame rate */
    h265_cbr.bit_rate = ((td_u64)3072 * (pic_width * pic_height)) / (1920 * 1080); /* 3072: 3M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h265_cbr = h265_cbr;
}

td_void video_venc_hi::comm_venc_h265_fixqp_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 frame_rate)
{
    ot_venc_h265_fixqp h265_fixqp;

    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_FIXQP;
    h265_fixqp.gop = gop;
    h265_fixqp.src_frame_rate = frame_rate;
    h265_fixqp.dst_frame_rate = frame_rate;
    h265_fixqp.i_qp = 25; /* 25 is a number */
    h265_fixqp.p_qp = 30; /* 30 is a number */
    h265_fixqp.b_qp = 32; /* 32 is a number */
    venc_chn_attr->rc_attr.h265_fixqp = h265_fixqp;    
}

td_void video_venc_hi::comm_venc_h265_vbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h265_vbr h265_vbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_VBR;
    h265_vbr.gop = gop;
    h265_vbr.stats_time = stats_time;
    h265_vbr.src_frame_rate = frame_rate;
    h265_vbr.dst_frame_rate = frame_rate;
    h265_vbr.max_bit_rate = ((td_u64)3072 * (pic_width * pic_height)) / (1920 * 1080); /* 3072: 3M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h265_vbr = h265_vbr;    
}

td_void video_venc_hi::comm_venc_h265_avbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h265_avbr h265_avbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_AVBR;
    h265_avbr.gop = gop;
    h265_avbr.stats_time = stats_time;
    h265_avbr.src_frame_rate = frame_rate;
    h265_avbr.dst_frame_rate = frame_rate;
    h265_avbr.max_bit_rate = ((td_u64)3072 * (pic_width * pic_height)) / (1920 * 1080); /* 3072: 3M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h265_avbr = h265_avbr;    
}

td_void video_venc_hi::comm_venc_set_h265_cvbr_bit_rate(ot_venc_chn_attr *venc_chn_attr, ot_venc_h265_cvbr *h265_cvbr, td_u32 frame_rate, ot_pic_size size)
{
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;

    switch (size) {
        case PIC_D1_NTSC:
            h265_cvbr->max_bit_rate = 1024 + 512 * frame_rate / 30;           /* 1024 512 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 + 512 * frame_rate / 30; /* 1024 512 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 256;                          /* 256 is a number */
            break;

        case PIC_720P:
            h265_cvbr->max_bit_rate = 1024 * 2 + 1024 * frame_rate / 30;           /* 1024 2 1024 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 * 2 + 1024 * frame_rate / 30; /* 1024 2 1024 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 512;                               /* 512 is a number */
            break;

        case PIC_1080P:
            h265_cvbr->max_bit_rate = 1024 * 2 + 2048 * frame_rate / 30;           /* 1024 2 2048 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 * 2 + 2048 * frame_rate / 30; /* 1024 2 2048 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 1024;                              /* 1024 is a number */
            break;

        case PIC_2304X1296:
            h265_cvbr->max_bit_rate = 1024 * 3 + 3072 * frame_rate / 30;           /* 1024 4 3072 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 * 3 + 3072 * frame_rate / 30; /* 1024 3 3072 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 1024 * 2;                          /* 1024 2 is a number */
            break;

        case PIC_2592X1944:
            h265_cvbr->max_bit_rate = 1024 * 4 + 3072 * frame_rate / 30;           /* 1024 4 3072 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 * 3 + 3072 * frame_rate / 30; /* 1024 3 3072 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 1024 * 2;                          /* 1024 2 is a number */
            break;

        case PIC_3840X2160:
            h265_cvbr->max_bit_rate = 1024 * 8 + 5120 * frame_rate / 30;           /* 1024 8 5120 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 * 5 + 5120 * frame_rate / 30; /* 1024 5 5120 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 1024 * 3;                          /* 1024 3 is a number */
            break;

        case PIC_4000X3000:
            h265_cvbr->max_bit_rate = 1024 * 12 + 5120 * frame_rate / 30;           /* 1024 12 5120 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 * 10 + 5120 * frame_rate / 30; /* 1024 10 5120 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 1024 * 4;                           /* 1024 4 is a number */
            break;

        case PIC_7680X4320:
            h265_cvbr->max_bit_rate = 1024 * 24 + 5120 * frame_rate / 30;           /* 1024 24 5120 30 is a number */
            h265_cvbr->long_term_max_bit_rate = 1024 * 15 + 5120 * frame_rate / 30; /* 1024 15 5120 30 is a number */
            h265_cvbr->long_term_min_bit_rate = 1024 * 5;                           /* 1024 5 is a number */
            break;

        default:
            h265_cvbr->max_bit_rate = ((td_u64)3072 * (pic_width * pic_height)) / /* 3072: 3M */
                (1920 * 1080); /* 1920, 1080: FHD */
            h265_cvbr->long_term_max_bit_rate = ((td_u64)3072 * (pic_width * pic_height)) / /* 3072: 3M */
                (1920 * 1080); /* 1920, 1080: FHD */
            h265_cvbr->long_term_min_bit_rate = 1024 * 5;                           /* 1024 5 is a number */
            break;
    }
}

td_void video_venc_hi::comm_venc_h265_cvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate, ot_pic_size size)
{
    ot_venc_h265_cvbr h265_cvbr;

    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_CVBR;
    h265_cvbr.gop = gop;
    h265_cvbr.stats_time = stats_time;
    h265_cvbr.src_frame_rate = frame_rate;
    h265_cvbr.dst_frame_rate = frame_rate;
    h265_cvbr.long_term_stats_time = 1;
    h265_cvbr.short_term_stats_time = stats_time;

    comm_venc_set_h265_cvbr_bit_rate(venc_chn_attr, &h265_cvbr, frame_rate, size);

    venc_chn_attr->rc_attr.h265_cvbr = h265_cvbr;    
}

td_void video_venc_hi::comm_venc_h265_qvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h265_qvbr h265_qvbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_QVBR;
    h265_qvbr.gop = gop;
    h265_qvbr.stats_time = stats_time;
    h265_qvbr.src_frame_rate = frame_rate;
    h265_qvbr.dst_frame_rate = frame_rate;
    h265_qvbr.target_bit_rate = ((td_u64)3072 * (pic_width * pic_height)) / /* 3072: 3M */
        (1920 * 1080); /* 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h265_qvbr = h265_qvbr;    
}

td_void video_venc_hi::comm_venc_h265_qpmap_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h265_qpmap h265_qpmap;

    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H265_QPMAP;
    h265_qpmap.gop = gop;
    h265_qpmap.stats_time = stats_time;
    h265_qpmap.src_frame_rate = frame_rate;
    h265_qpmap.dst_frame_rate = frame_rate;
    h265_qpmap.qpmap_mode = OT_VENC_RC_QPMAP_MODE_MEAN_QP;
    venc_chn_attr->rc_attr.h265_qpmap = h265_qpmap;    
}

td_void video_venc_hi::comm_venc_h264_abr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h264_abr h264_abr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_ABR;
    h264_abr.gop = gop;
    h264_abr.stats_time = stats_time;     /* stream rate statics time(s) */
    h264_abr.src_frame_rate = frame_rate; /* input (vi) frame rate */
    h264_abr.dst_frame_rate = frame_rate; /* target frame rate */
    h264_abr.vbv_buf_delay  = 50; // 50: vbv_buf_delay
    h264_abr.bit_rate = ((td_u64)4096 * (pic_width * pic_height)) / (1920 * 1080); /* 4096: 4M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h264_abr = h264_abr;    
}

td_void video_venc_hi::comm_venc_h264_cbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h264_cbr h264_cbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_CBR;
    h264_cbr.gop = gop;
    h264_cbr.stats_time = stats_time;     /* stream rate statics time(s) */
    h264_cbr.src_frame_rate = frame_rate; /* input (vi) frame rate */
    h264_cbr.dst_frame_rate = frame_rate; /* target frame rate */
    h264_cbr.bit_rate = ((td_u64)4096 * (pic_width * pic_height)) / (1920 * 1080); /* 4096: 4M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h264_cbr = h264_cbr;    
}

td_void video_venc_hi::comm_venc_h264_fixqp_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 frame_rate)
{
    ot_venc_h264_fixqp h264_fixqp;

    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_FIXQP;
    h264_fixqp.gop = gop;
    h264_fixqp.src_frame_rate = frame_rate;
    h264_fixqp.dst_frame_rate = frame_rate;
    h264_fixqp.i_qp = 25; /* 25 is a number */
    h264_fixqp.p_qp = 30; /* 30 is a number */
    h264_fixqp.b_qp = 32; /* 32 is a number */
    venc_chn_attr->rc_attr.h264_fixqp = h264_fixqp;    
}

td_void video_venc_hi::comm_venc_h264_vbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h264_vbr h264_vbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_VBR;
    h264_vbr.gop = gop;
    h264_vbr.stats_time = stats_time;
    h264_vbr.src_frame_rate = frame_rate;
    h264_vbr.dst_frame_rate = frame_rate;
    h264_vbr.max_bit_rate = ((td_u64)4096 * (pic_width * pic_height)) / (1920 * 1080); /* 4096: 4M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h264_vbr = h264_vbr;    
}

td_void video_venc_hi::comm_venc_h264_avbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h264_avbr h264_avbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_AVBR;
    h264_avbr.gop = gop;
    h264_avbr.stats_time = stats_time;
    h264_avbr.src_frame_rate = frame_rate;
    h264_avbr.dst_frame_rate = frame_rate;
    h264_avbr.max_bit_rate = ((td_u64)4096 * (pic_width * pic_height)) / (1920 * 1080); /* 4096: 4M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h264_avbr = h264_avbr;    
}

td_void video_venc_hi::comm_venc_set_h264_cvbr_bit_rate(ot_venc_chn_attr *venc_chn_attr, ot_venc_h264_cvbr *h264_cvbr,
    td_u32 frame_rate, ot_pic_size size)
{
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;

    switch (size) {
        case PIC_D1_NTSC:
            h264_cvbr->max_bit_rate = 1024 + 512 * frame_rate / 30;           /* 1024 512 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 + 512 * frame_rate / 30; /* 1024 512 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 256;                          /* 256 is a number */
            break;
        case PIC_720P:
            h264_cvbr->max_bit_rate = 1024 * 2 + 1024 * frame_rate / 30;           /* 1024 2 1024 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 * 2 + 1024 * frame_rate / 30; /* 1024 2 1024 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 512;                               /* 512 is a number */
            break;
        case PIC_1080P:
            h264_cvbr->max_bit_rate = 1024 * 2 + 2048 * frame_rate / 30;           /* 1024 2 2048 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 * 2 + 2048 * frame_rate / 30; /* 1024 2 2048 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 1024;                              /* 1024 is a number */
            break;
        case PIC_2304X1296:
            h264_cvbr->max_bit_rate = 1024 * 3 + 3072 * frame_rate / 30;           /* 1024 4 3072 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 * 3 + 3072 * frame_rate / 30; /* 1024 3 3072 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 1024 * 2;                          /* 1024 2 is a number */
            break;
        case PIC_2592X1944:
            h264_cvbr->max_bit_rate = 1024 * 4 + 3072 * frame_rate / 30;           /* 1024 4 3072 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 * 3 + 3072 * frame_rate / 30; /* 1024 3 3072 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 1024 * 2;                          /* 1024 2 is a number */
            break;
        case PIC_3840X2160:
            h264_cvbr->max_bit_rate = 1024 * 8 + 5120 * frame_rate / 30;           /* 1024 8 5120 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 * 5 + 5120 * frame_rate / 30; /* 1024 5 5120 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 1024 * 3;                          /* 1024 3 is a number */
            break;
        case PIC_4000X3000:
            h264_cvbr->max_bit_rate = 1024 * 12 + 5120 * frame_rate / 30;           /* 1024 12 5120 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 * 10 + 5120 * frame_rate / 30; /* 1024 10 5120 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 1024 * 4;                           /* 1024 4 is a number */
            break;
        case PIC_7680X4320:
            h264_cvbr->max_bit_rate = 1024 * 24 + 5120 * frame_rate / 30;           /* 1024 24 5120 30 is a number */
            h264_cvbr->long_term_max_bit_rate = 1024 * 15 + 5120 * frame_rate / 30; /* 1024 15 5120 30 is a number */
            h264_cvbr->long_term_min_bit_rate = 1024 * 5;                           /* 1024 5 is a number */
            break;
        default:
            h264_cvbr->max_bit_rate = ((td_u64)4096 * (pic_width * pic_height)) /   /* 4096: 4M */
                (1920 * 1080); /* 1920, 1080: FHD */
            h264_cvbr->long_term_max_bit_rate = ((td_u64)4096 * (pic_width * pic_height)) /   /* 4096: 4M */
                (1920 * 1080); /* 1920, 1080: FHD */
            h264_cvbr->long_term_min_bit_rate = 1024 * 5;                           /* 1024 5 is a number */
            break;
    }    
}

td_void video_venc_hi::comm_venc_h264_cvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate, ot_pic_size size)
{
    ot_venc_h264_cvbr h264_cvbr;

    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_CVBR;
    h264_cvbr.gop = gop;
    h264_cvbr.stats_time = stats_time;
    h264_cvbr.src_frame_rate = frame_rate;
    h264_cvbr.dst_frame_rate = frame_rate;
    h264_cvbr.long_term_stats_time = 1;
    h264_cvbr.short_term_stats_time = stats_time;

    comm_venc_set_h264_cvbr_bit_rate(venc_chn_attr, &h264_cvbr, frame_rate, size);

    venc_chn_attr->rc_attr.h264_cvbr = h264_cvbr;    
}

td_void video_venc_hi::comm_venc_h264_qvbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h264_qvbr h264_qvbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_QVBR;
    h264_qvbr.gop = gop;
    h264_qvbr.stats_time = stats_time;
    h264_qvbr.src_frame_rate = frame_rate;
    h264_qvbr.dst_frame_rate = frame_rate;
    h264_qvbr.target_bit_rate = ((td_u64)4096 * (pic_width * pic_height)) / /* 4096: 4M */
        (1920 * 1080); /* 1920, 1080: FHD */
    venc_chn_attr->rc_attr.h264_qvbr = h264_qvbr;    
}

td_void video_venc_hi::comm_venc_h264_qpmap_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 gop, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_h264_qpmap h264_qpmap;

    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_H264_QPMAP;
    h264_qpmap.gop = gop;
    h264_qpmap.stats_time = stats_time;
    h264_qpmap.src_frame_rate = frame_rate;
    h264_qpmap.dst_frame_rate = frame_rate;

    venc_chn_attr->rc_attr.h264_qpmap = h264_qpmap;    
}

td_void video_venc_hi::comm_venc_mjpeg_fixqp_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 frame_rate)
{
    ot_venc_mjpeg_fixqp mjpege_fixqp;

    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_MJPEG_FIXQP;
    mjpege_fixqp.qfactor = 50; /* 50 is a number */
    mjpege_fixqp.src_frame_rate = frame_rate;
    mjpege_fixqp.dst_frame_rate = frame_rate;

    venc_chn_attr->rc_attr.mjpeg_fixqp = mjpege_fixqp;    
}

td_void video_venc_hi::comm_venc_mjpeg_cbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_mjpeg_cbr mjpege_cbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_MJPEG_CBR;
    mjpege_cbr.stats_time = stats_time;
    mjpege_cbr.src_frame_rate = frame_rate;
    mjpege_cbr.dst_frame_rate = frame_rate;
    mjpege_cbr.bit_rate = ((td_u64)20480 * (pic_width * pic_height)) / (1920 * 1080); /* 20480: 20M 1920, 1080: FHD */
    venc_chn_attr->rc_attr.mjpeg_cbr = mjpege_cbr;    
}

td_void video_venc_hi::comm_venc_mjpeg_vbr_param_init(ot_venc_chn_attr *venc_chn_attr, td_u32 stats_time, td_u32 frame_rate)
{
    ot_venc_mjpeg_vbr mjpeg_vbr;
    td_u32  pic_width;
    td_u32  pic_height;

    pic_width = venc_chn_attr->venc_attr.pic_width;
    pic_height = venc_chn_attr->venc_attr.pic_height;
    venc_chn_attr->rc_attr.rc_mode = OT_VENC_RC_MODE_MJPEG_VBR;
    mjpeg_vbr.stats_time = stats_time;
    mjpeg_vbr.src_frame_rate = frame_rate;
    mjpeg_vbr.dst_frame_rate = frame_rate;
    mjpeg_vbr.max_bit_rate = ((td_u64)20480 * (pic_width * pic_height)) / /* 20480: 20M */
        (1920 * 1080); /* 1920, 1080: FHD */
    venc_chn_attr->rc_attr.mjpeg_vbr = mjpeg_vbr;    
}

td_s32 video_venc_hi::comm_venc_jpeg_param_init(ot_venc_chn_attr *venc_chn_attr)
{
    ot_venc_jpeg_attr jpeg_attr;
    jpeg_attr.dcf_en = TD_FALSE;
    jpeg_attr.mpf_cfg.large_thumbnail_num = 0;
    jpeg_attr.recv_mode = OT_VENC_PIC_RECV_SINGLE;

    venc_chn_attr->venc_attr.jpeg_attr = jpeg_attr;

    return TD_SUCCESS;
}

} // namespace hiMppMedia