/* kate: replace-tabs true; indent-width 4
 *
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../engine_headers/host1x.h"
#include "../engine_headers/nvdec.h"
#include "../uapi_headers/tegra_drm.h"
#include "nvdec.h"
#include <libdrm/drm.h>
#include <linux/kernel.h>

#include <unistd.h>

static const uint8_t quant_mat_8x8intra[64] = {
    8, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83
};

static const uint8_t quant_mat_8x8nonintra[64] = {
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16
};

NvdecOp::NvdecOp()
    : _slice_data(nullptr)
{
}

NvdecDevice::NvdecDevice(DrmDevice& dev)
    : _dev(dev)
    , _context(0)
    , _syncpt(0xffffffff)
    , _cmd_bo(dev)
    , _config_bo(dev)
    , _status_bo(dev)
    , _history_bo(dev)
    , _mbhist_bo(dev)
    , _coloc_bo(dev)
{
}

NvdecDevice::~NvdecDevice()
{
    if (_syncpt != 0xffffffff)
        _dev.free_syncpoint(_syncpt);
    if (_context)
        _dev.close_channel(_context);
}

int NvdecDevice::open()
{
    char tmp[30] = {0};
    FILE *fp;
    int err;

    if (_context)
        return 0;

    fp = fopen("/sys/devices/soc0/soc_id", "r");
    if (!fp) {
        perror("Failed to open /sys/devices/soc0/soc_id");
        return -errno;
    }

    fread(tmp, 1, sizeof(tmp)-1, fp);
    fclose(fp);
    if (!strcmp(tmp, "33\n"))
        _is210 = true;
    else if (!strcmp(tmp, "24\n"))
        _is210 = false;
    else if (!strcmp(tmp, "25\n"))
        _is210 = false;
    else {
        printf("Unknown chip %s\n", tmp);
        return -1;
    }

    err = _dev.open_channel(HOST1X_CLASS_NVDEC, &_context);
    if (err == -1) {
        perror("Channel open failed");
        return err;
    }

    err = _cmd_bo.allocate(0x1000);
    if (err)
        return err;

    err = _config_bo.allocate(0x1000);
    if (err)
        return err;

    err = _config_bo.channelMap(_context, false);
    if (err)
        return err;

    err = _status_bo.allocate(0x1000);
    if (err)
        return err;

    err = _mbhist_bo.allocate(32768);
    if (err)
        return err;

    err = _history_bo.allocate(256 * 1024);
    if (err)
        return err;

    err = _coloc_bo.allocate(512 * 1024);
    if (err)
        return err;

    err = _dev.allocate_syncpoint(_context, &_syncpt);
    if (err == -1) {
        perror("Syncpt get failed");
        return err;
    }

    return 0;
}

static void runMPEG2(void *cfg, NvdecOp &op, std::vector<GemBuffer *> &surfaces)
{
    nvdec_mpeg2_pic_s* c = (nvdec_mpeg2_pic_s*)cfg;

    memset(c, 0, sizeof(*c));

    c->stream_len = op.sliceDataLength();
    c->slice_count = op.numSlices();
    c->gob_height = 0;

    const VAPictureParameterBufferMPEG2 &pp = op.mpeg2().picture_parameters;

    c->FrameWidth = pp.horizontal_size;
    c->FrameHeight = pp.vertical_size;
    c->picture_structure = pp.picture_coding_extension.bits.picture_structure;
    c->picture_coding_type = pp.picture_coding_type;
    c->intra_dc_precision = pp.picture_coding_extension.bits.intra_dc_precision;
    c->frame_pred_frame_dct = pp.picture_coding_extension.bits.frame_pred_frame_dct;
    c->concealment_motion_vectors = pp.picture_coding_extension.bits.concealment_motion_vectors;
    c->intra_vlc_format = pp.picture_coding_extension.bits.intra_vlc_format;
    c->f_code[0] = (pp.f_code >> 12) & 0xf; // 0,0
    c->f_code[1] = (pp.f_code >> 8) & 0xf; // 0,1
    c->f_code[2] = (pp.f_code >> 4) & 0xf; // 1,0
    c->f_code[3] = (pp.f_code >> 0) & 0xf; // 1,1

    c->PicWidthInMbs = __ALIGN_KERNEL(pp.horizontal_size, 16) >> 4;
    c->FrameHeightInMbs = __ALIGN_KERNEL(pp.vertical_size, 16) >> 4;
    c->pitch_luma = op.output().pitch;      // In bytes
    c->pitch_chroma = op.output().pitch;    // In bytes
    c->luma_top_offset = 0;
    c->chroma_top_offset = 0;
    c->luma_frame_offset = 0;
    c->chroma_frame_offset = 0;
    c->luma_bot_offset = 0;
    c->chroma_bot_offset = 0;
    c->output_memory_layout = 0;
    c->ref_memory_layout[0] = 0;
    c->ref_memory_layout[1] = 0;

    c->alternate_scan = pp.picture_coding_extension.bits.alternate_scan;
    c->secondfield = pp.picture_coding_extension.bits.picture_structure != 3 &&
        !pp.picture_coding_extension.bits.is_first_field;
    c->q_scale_type = pp.picture_coding_extension.bits.q_scale_type;
    c->top_field_first = pp.picture_coding_extension.bits.top_field_first;

    if (op.mpeg2().iq_matrix.load_intra_quantiser_matrix)
        memcpy(c->quant_mat_8x8intra, op.mpeg2().iq_matrix.intra_quantiser_matrix, 64);
    else
        memcpy(c->quant_mat_8x8intra, quant_mat_8x8intra, sizeof(c->quant_mat_8x8intra));
    if (op.mpeg2().iq_matrix.load_non_intra_quantiser_matrix)
        memcpy(c->quant_mat_8x8nonintra, op.mpeg2().iq_matrix.non_intra_quantiser_matrix, 64);
    else
        memcpy(c->quant_mat_8x8nonintra, quant_mat_8x8nonintra, sizeof(c->quant_mat_8x8nonintra));

    surfaces.push_back(op.output().bo);

    if (op.mpeg2().forward_reference)
        surfaces.push_back(op.mpeg2().forward_reference);
    else if (op.mpeg2().backward_reference)
        surfaces.push_back(op.mpeg2().backward_reference);
    else
        surfaces.push_back(op.output().bo);

    if (op.mpeg2().backward_reference)
        surfaces.push_back(op.mpeg2().backward_reference);
    else if (op.mpeg2().forward_reference && false)
        surfaces.push_back(op.mpeg2().forward_reference);
    else
        surfaces.push_back(op.output().bo);
}

NvdecDevice::SlotManager::SlotManager() {
    for (auto &slot : slots)
        slot = VA_INVALID_SURFACE;
}

void NvdecDevice::SlotManager::clean(const VAPictureH264 *refs, size_t num_refs) {
    for (size_t slot = 0; slot < slots.size(); slot++) {
        if (slots[slot] == VA_INVALID_SURFACE)
            continue;

        bool found = false;
        for (size_t i = 0; i < num_refs; i++) {
            if (refs[i].picture_id == slots[i]) {
                found = true;
                break;
            }
        }

        if (!found)
            slots[slot] = VA_INVALID_SURFACE;
    }

    fprintf(stderr, "cleaning. left: ");
    for (size_t slot = 0; slot < slots.size(); slot++) {
        if (slots[slot] != VA_INVALID_SURFACE)
            fprintf(stderr, "(pic=%d slot=%d) ", slots[slot], slot);
    }
    fprintf(stderr, "\n");
}

int NvdecDevice::SlotManager::get(VASurfaceID surface, bool insert) {
    int unused_slot = -1;

    for (size_t slot = 0; slot < slots.size(); slot++) {
        if (slots[slot] == VA_INVALID_SURFACE && unused_slot == -1)
            unused_slot = slot;

        if (slots[slot] == surface)
            return slot;
    }

    if (!insert)
        return -1;

    if (unused_slot >= 0) {
        slots[unused_slot] = surface;
        return unused_slot;
    } else {
        printf("Too many references!\n");
        return -1;
    }
}

void NvdecDevice::runH264(void *cfg, NvdecOp &op, std::vector<GemBuffer *> &surfaces)
{
    const VAPictureParameterBufferH264 &pp = op.h264().picture_parameters;
    nvdec_h264_pic_s* c = (nvdec_h264_pic_s*)cfg;
    uint32_t slot;

    //_slots.clean(pp.ReferenceFrames, pp.num_ref_frames);
    slot = _slots.get(pp.CurrPic.picture_id, true);

    memset(c, 0, sizeof(*c));

    c->stream_len = op.sliceDataLength();
    c->slice_count = op.numSlices();
    c->gob_height = 0;

    c->PicWidthInMbs = pp.picture_width_in_mbs_minus1 + 1;
    c->FrameHeightInMbs = pp.picture_height_in_mbs_minus1 + 1;
    c->pitch_luma = op.output().pitch;      // In bytes
    c->pitch_chroma = op.output().pitch;    // In bytes
    c->luma_top_offset = 0;
    c->chroma_top_offset = 0;
    c->luma_frame_offset = 0;
    c->chroma_frame_offset = 0;
    c->luma_bot_offset = 0;
    c->chroma_bot_offset = 0;
    c->output_memory_layout = 0;

    c->constrained_intra_pred_flag = pp.pic_fields.bits.constrained_intra_pred_flag;
    c->ref_pic_flag = pp.pic_fields.bits.reference_pic_flag;
    c->CurrFieldOrderCnt[0] = pp.CurrPic.TopFieldOrderCnt;
    c->CurrFieldOrderCnt[1] = pp.CurrPic.BottomFieldOrderCnt;
    c->chroma_qp_index_offset = pp.chroma_qp_index_offset;
    c->second_chroma_qp_index_offset = pp.second_chroma_qp_index_offset;
    c->CurrPicIdx = slot;
    c->CurrColIdx = slot;
    c->frame_num = pp.frame_num;
    c->lossless_ipred8x8_filter_enable = 1;

    //c->qpprime_y_zero_transform_bypass_flag = 0;
    memcpy(c->WeightScale, op.h264().iq_matrix.ScalingList4x4, sizeof(c->WeightScale));
    memcpy(c->WeightScale8x8, op.h264().iq_matrix.ScalingList8x8, sizeof(c->WeightScale8x8));

    c->chroma_format_idc = pp.seq_fields.bits.chroma_format_idc;
    c->log2_max_frame_num_minus4 = pp.seq_fields.bits.log2_max_frame_num_minus4;
    c->pic_order_cnt_type = pp.seq_fields.bits.pic_order_cnt_type;
    c->log2_max_pic_order_cnt_lsb_minus4 = pp.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4;
    c->delta_pic_order_always_zero_flag = pp.seq_fields.bits.delta_pic_order_always_zero_flag;
    c->frame_mbs_only_flag = pp.seq_fields.bits.frame_mbs_only_flag;
    c->direct_8x8_inference_flag = pp.seq_fields.bits.frame_mbs_only_flag;
    c->tileFormat = 0;

    c->entropy_coding_mode_flag = pp.pic_fields.bits.entropy_coding_mode_flag;
    c->pic_order_present_flag = pp.pic_fields.bits.pic_order_present_flag;
    c->num_ref_idx_l0_active_minus1 = op.h264().slice_parameters.num_ref_idx_l0_active_minus1;
    c->num_ref_idx_l1_active_minus1 = op.h264().slice_parameters.num_ref_idx_l1_active_minus1;
    c->weighted_pred_flag = pp.pic_fields.bits.weighted_pred_flag;
    c->weighted_bipred_idc = pp.pic_fields.bits.weighted_bipred_idc;
    c->pic_init_qp_minus26 = pp.pic_init_qp_minus26;
    c->deblocking_filter_control_present_flag = pp.pic_fields.bits.deblocking_filter_control_present_flag;
    c->redundant_pic_cnt_present_flag = pp.pic_fields.bits.redundant_pic_cnt_present_flag;
    c->transform_8x8_mode_flag = pp.pic_fields.bits.transform_8x8_mode_flag;
    c->MbaffFrameFlag = pp.seq_fields.bits.mb_adaptive_frame_field_flag;
    c->field_pic_flag = pp.pic_fields.bits.field_pic_flag;
    c->bottom_field_flag = !!(pp.CurrPic.flags & VA_PICTURE_H264_BOTTOM_FIELD);

    c->HistBufferSize = _history_bo.size() / 256;
    c->mbhist_buffer_size = _mbhist_bo.size();

    surfaces.resize(17, op.output().bo);

    // surfaces[slot] = op.output().bo;

    static int frame = 0;

    // fprintf(stderr, "Frame %d: pic=%d slot=%d refs=%d ", frame++, pp.CurrPic.picture_id, slot, pp.num_ref_frames);

    for (uint8_t i = 0; i < pp.num_ref_frames; i++) {
        VAPictureH264 ref = pp.ReferenceFrames[i];
        nvdec_dpb_entry_s &dpb = c->dpb[i];

        if (ref.flags & VA_PICTURE_H264_INVALID)
            continue;

        uint32_t state = 0;
        if (ref.flags & VA_PICTURE_H264_TOP_FIELD)
            state |= (1<<0);
        if (ref.flags & VA_PICTURE_H264_BOTTOM_FIELD)
            state |= (1<<1);
        if (!state)
            state = 3;

        dpb.FrameIdx = ref.frame_idx;
        dpb.state = state;

        dpb.is_field = !!(ref.flags & (VA_PICTURE_H264_TOP_FIELD | VA_PICTURE_H264_BOTTOM_FIELD));;
        dpb.is_long_term = !!(ref.flags & VA_PICTURE_H264_LONG_TERM_REFERENCE);

        uint32_t marking = dpb.is_long_term ? 2 : 1;
        dpb.top_field_marking = (state & (1<<0)) ? marking : 0;
        dpb.bottom_field_marking = (state & (1<<1)) ? marking : 0;

        /* These probably need some mangling depending on top/bottom field? */
        dpb.FieldOrderCnt[0] = ref.TopFieldOrderCnt;
        dpb.FieldOrderCnt[1] = ref.BottomFieldOrderCnt;

        int ref_slot = _slots.get(ref.picture_id, false);
        // fprintf(stderr, "%d(pic=%d slot=%d f=0x%x) ", i, ref.picture_id, ref_slot, ref.flags);
        if (ref_slot == -1) {
            printf("Reference was not decoded yet!\n");
            continue;
        }

        dpb.index = ref_slot;
        dpb.col_idx = ref_slot;

        surfaces[ref_slot] = op.h264().references[i];
    }

    // fprintf(stderr, "\n");

    // {
    //     char tmp[100];
    //     sprintf(tmp, "vapicsetup.%d.bin", frame);
    //     FILE *fp = fopen(tmp, "w");
    //     fwrite(c, sizeof(*c), 1, fp);
    //     fclose(fp);
    // }

}

int NvdecDevice::run(NvdecOp& op)
{
    int err, i;
    uint32_t* cmd = (uint32_t*)_cmd_bo.map();
    void *c = _config_bo.map();
    std::vector<drm_tegra_reloc> relocs;
    std::vector<drm_tegra_submit_buf> relocs_new;
    uint32_t application_id, codec_type;
    std::vector<GemBuffer *> surfaces;

    if (!c || !cmd)
        return 1;

    switch (op.codec()) {
    case NvdecCodec::MPEG2:
        application_id = NVC5B0_SET_APPLICATION_ID_ID_MPEG12;
        codec_type = NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG2;
        runMPEG2(c, op, surfaces);
        break;
    case NvdecCodec::H264:
        application_id = NVC5B0_SET_APPLICATION_ID_ID_H264;
        codec_type = NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_H264;
        runH264(c, op, surfaces);
        break;
    default:
        printf("Unsupported codec\n");
        return 1;
    }

    memset(cmd, 0, 0x1000);
    i = 0;

#define M(name, value)                                                \
    do {                                                              \
        cmd[i++] = host1x_opcode_incr(NVDEC_UCLASS_METHOD_OFFSET, 2); \
        cmd[i++] = (name) >> 2;                                       \
        cmd[i++] = (value);                                           \
    } while (0);
#define BO(h, offs, rw)                           \
    do {                                          \
        if (!h) { \
            printf("Internal error: BO is null\n"); \
            return -1; \
        } \
        err = (h)->channelMap(_context, (rw));    \
        if (err) {                                \
            perror("Surface mapping failed");     \
            return err;                           \
        }                                         \
                                                  \
        drm_tegra_submit_buf __buf = { 0 };       \
        __buf.mapping_id = (h)->mappingId(_context);      \
        __buf.reloc.target_offset = (offs);       \
        __buf.reloc.gather_offset_words = i - 1;  \
        __buf.reloc.shift = 8;                    \
        relocs_new.push_back(__buf);              \
                                                  \
        drm_tegra_reloc __reloc;                  \
        __reloc.cmdbuf.handle = _cmd_bo.handle(); \
        __reloc.cmdbuf.offset = (i - 1) * 4;      \
        __reloc.target.handle = (h)->handle();    \
        __reloc.target.offset = (offs);           \
        __reloc.shift = 8;                        \
        __reloc.pad = 0;                          \
        relocs.push_back(__reloc);                \
    } while (0);

    M(NVC5B0_SET_APPLICATION_ID, application_id);
    M(NVC5B0_SET_CONTROL_PARAMS,
        codec_type | NVC5B0_SET_CONTROL_PARAMS_GPTIMER_ON | NVC5B0_SET_CONTROL_PARAMS_ERR_CONCEAL_ON | NVC5B0_SET_CONTROL_PARAMS_ERROR_FRM_IDX(0));
    M(NVC5B0_SET_DRV_PIC_SETUP_OFFSET, 0xdeadbeef);
    BO(&_config_bo, 0, false);
    M(NVC5B0_SET_IN_BUF_BASE_OFFSET, 0xdeadbeef);
    BO(op.sliceData(), 0, false);
    M(NVC5B0_SET_SLICE_OFFSETS_BUF_OFFSET, 0xdeadbeef);
    BO(op.sliceDataOffsets(), 0, false);

    for (size_t surf_i = 0; surf_i < surfaces.size(); surf_i++) {
        if (!surfaces[surf_i])
            continue;

        M(NVC5B0_SET_PICTURE_LUMA_OFFSET0 + 4*surf_i, 0xdeadbeef);
        BO(surfaces[surf_i], 0, true);
        M(NVC5B0_SET_PICTURE_CHROMA_OFFSET0 + 4*surf_i, 0xdeadbeef);
        BO(surfaces[surf_i], op.output().pitch * op.output().paddedHeight(), true);
    }

    if (op.codec() == NvdecCodec::H264) {
        M(NVC5B0_SET_COLOC_DATA_OFFSET, 0xdeadbeef);
        BO(&_coloc_bo, 0, true);
        M(NVC5B0_SET_HISTORY_OFFSET, 0xdeadbeef);
        BO(&_history_bo, 0, true);
        M(NVC5B0_H264_SET_MBHIST_BUF_OFFSET, 0xdeadbeef);
        BO(&_mbhist_bo, 0, true);
    }

    M(NVC5B0_SET_NVDEC_STATUS_OFFSET, 0xdeadbeef);
    BO(&_status_bo, 0, true);

    static unsigned int pidx = 0;
    M(NVC5B0_SET_PICTURE_INDEX, pidx++);

    M(NVC5B0_EXECUTE, NVC5B0_EXECUTE_AWAKEN_ENABLE);

    cmd[i++] = host1x_opcode_nonincr(0, 1);
    cmd[i++] = _syncpt | (1 << (_is210 ? 8 : 10));

    if (_dev.isNewApi()) {
        drm_tegra_submit_cmd submit_cmds[1] = { 0 };
        submit_cmds[0].type = DRM_TEGRA_SUBMIT_CMD_GATHER_UPTR;
        submit_cmds[0].gather_uptr.words = i;

        drm_tegra_channel_submit submit = { 0 };
        submit.channel_ctx = _context;
        submit.num_bufs = relocs_new.size();
        submit.num_cmds = 1;
        submit.gather_data_words = i;
        submit.bufs_ptr = (__u64)&relocs_new[0];
        submit.cmds_ptr = (__u64)&submit_cmds[0];
        submit.gather_data_ptr = (__u64)&cmd[0];
        submit.syncpt_incr.id = _syncpt;
        submit.syncpt_incr.num_incrs = 1;

        err = _dev.ioctl(DRM_IOCTL_TEGRA_CHANNEL_SUBMIT, &submit);
        if (err == -1) {
            perror("Submit failed");
            return err;
        }

        err = _dev.waitSyncpoint(_syncpt, submit.syncpt_incr.fence_value);
        if (err)
            return err;
    } else {
        drm_tegra_syncpt incr;
        incr.id = _syncpt;
        incr.incrs = 1;

        drm_tegra_cmdbuf cmdbuf;
        cmdbuf.handle = _cmd_bo.handle();
        cmdbuf.offset = 0;
        cmdbuf.words = i;

        drm_tegra_submit submit;
        memset(&submit, 0, sizeof(submit));
        submit.context = _context;
        submit.num_syncpts = 1;
        submit.num_cmdbufs = 1;
        submit.num_relocs = relocs.size();
        submit.syncpts = (uintptr_t)&incr;
        submit.cmdbufs = (uintptr_t)&cmdbuf;
        submit.relocs = (uintptr_t)&relocs[0];

        err = _dev.ioctl(DRM_IOCTL_TEGRA_SUBMIT, &submit);
        if (err == -1) {
            perror("Submit failed");
            return err;
        }

        return _dev.waitSyncpoint(_syncpt, submit.fence);
    }

    return 0;
}
