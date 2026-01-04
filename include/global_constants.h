#ifndef GLOBAL_CONSTANTS_H
#define GLOBAL_CONSTANTS_H
#include <ot_common_video.h>
namespace hiMppMedia {

typedef enum {
    PIC_CIF,
    PIC_360P,    /* 640 * 360 */
    PIC_D1_PAL,  /* 720 * 576 */
    PIC_D1_NTSC, /* 720 * 480 */
    PIC_960H,      /* 960 * 576 */
    PIC_720P,    /* 1280 * 720 */
    PIC_1080P,   /* 1920 * 1080 */
    PIC_480P,
    PIC_576P,
    PIC_800X600,
    PIC_1024X768,
    PIC_1280X1024,
    PIC_1366X768,
    PIC_1440X900,
    PIC_1280X800,
    PIC_1600X1200,
    PIC_1680X1050,
    PIC_1920X1200,
    PIC_640X480,
    PIC_1920X2160,
    PIC_2304X1296,
    PIC_2560X1440,
    PIC_2560X1600,
    PIC_2592X1520,
    PIC_2688X1520,
    PIC_2592X1944,
    PIC_3840X2160,
    PIC_4096X2160,
    PIC_3000X3000,
    PIC_4000X3000,
    PIC_6080X2800,
    PIC_7680X4320,
    PIC_3840X8640,
    PIC_2880X1620,
    PIC_BUTT
} ot_pic_size;

/* The order of g_sample_pic_size's element must be consistent with the enum value defined in "ot_pic_size". */
extern ot_size pic_size_array[PIC_BUTT];

}

#endif // GLOBAL_CONSTANTS_H