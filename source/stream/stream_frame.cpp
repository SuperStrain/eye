#include "stream_frame.h"
#include <cstring>

StreamFrame::StreamFrame(VencChannel chn, StreamType type, CodecType codec, const ot_venc_stream& src)
    : channel_(chn), type_(type), codec_type_(codec) {

    timestamp_ = (src.pack_cnt > 0) ? src.pack[0].pts : 0;

    pack_array_.resize(src.pack_cnt);
    pack_buffers_.reserve(src.pack_cnt);
    stream_.pack_cnt = src.pack_cnt;
    stream_.pack = pack_array_.data();

    for (uint32_t i = 0; i < src.pack_cnt; ++i) {
        const ot_venc_pack& src_pack = src.pack[i];

        td_u32 offset = src_pack.offset;
        td_u32 data_len = src_pack.len - offset;

        std::vector<uint8_t> buf(data_len);
        memcpy(buf.data(), src_pack.addr + offset, data_len);

        pack_array_[i].addr = buf.data();
        pack_array_[i].phys_addr = 0;
        pack_array_[i].len = data_len;
        pack_array_[i].pts = src_pack.pts;
        pack_array_[i].is_frame_end = src_pack.is_frame_end;
        pack_array_[i].data_type = src_pack.data_type;
        pack_array_[i].offset = 0;
        pack_array_[i].stuff03_disable = src_pack.stuff03_disable;
        pack_array_[i].layer_id = src_pack.layer_id;
        pack_array_[i].is_lu_begin = src_pack.is_lu_begin;
        pack_array_[i].data_num = src_pack.data_num;

        pack_buffers_.push_back(std::move(buf));
    }
}

StreamFrame::~StreamFrame() {
}
