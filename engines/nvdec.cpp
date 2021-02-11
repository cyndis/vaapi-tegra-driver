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
    , _forward_reference(nullptr)
    , _backward_reference(nullptr)
{
}

NvdecDevice::NvdecDevice(DrmDevice& dev)
    : _dev(dev)
    , _context(0)
    , _syncpt(0xffffffff)
    , _cmd_bo(dev)
    , _config_bo(dev)
    , _status_bo(dev)
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

    err = _config_bo.allocate(sizeof(nvdec_mpeg2_pic_s));
    if (err)
        return err;

    err = _config_bo.channelMap(_context, false);
    if (err)
        return err;

    err = _status_bo.allocate(0x1000);
    if (err)
        return err;

    err = _dev.allocate_syncpoint(_context, &_syncpt);
    if (err == -1) {
        perror("Syncpt get failed");
        return err;
    }

    return 0;
}

int NvdecDevice::run(NvdecOp& op)
{
    int err, i;
    nvdec_mpeg2_pic_s* c = (nvdec_mpeg2_pic_s*)_config_bo.map();
    uint32_t* cmd = (uint32_t*)_cmd_bo.map();
    std::vector<drm_tegra_reloc> relocs;
    std::vector<drm_tegra_submit_buf> relocs_new;

    if (!c || !cmd)
        return 1;

    memset(c, 0, sizeof(*c));

    c->stream_len = op.sliceDataLength();
    c->slice_count = op.numSlices();
    c->gob_height = 0;

    c->FrameWidth = op.pictureParameters().horizontal_size;
    c->FrameHeight = op.pictureParameters().vertical_size;
    c->picture_structure = op.pictureParameters().picture_coding_extension.bits.picture_structure;
    c->picture_coding_type = op.pictureParameters().picture_coding_type;
    c->intra_dc_precision = op.pictureParameters().picture_coding_extension.bits.intra_dc_precision;
    c->frame_pred_frame_dct = op.pictureParameters().picture_coding_extension.bits.frame_pred_frame_dct;
    c->concealment_motion_vectors = op.pictureParameters().picture_coding_extension.bits.concealment_motion_vectors;
    c->intra_vlc_format = op.pictureParameters().picture_coding_extension.bits.intra_vlc_format;
    c->f_code[0] = (op.pictureParameters().f_code >> 12) & 0xf; // 0,0
    c->f_code[1] = (op.pictureParameters().f_code >> 8) & 0xf; // 0,1
    c->f_code[2] = (op.pictureParameters().f_code >> 4) & 0xf; // 1,0
    c->f_code[3] = (op.pictureParameters().f_code >> 0) & 0xf; // 1,1

    c->PicWidthInMbs = __ALIGN_KERNEL(op.pictureParameters().horizontal_size, 16) >> 4;
    c->FrameHeightInMbs = __ALIGN_KERNEL(op.pictureParameters().vertical_size, 16) >> 4;
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

    c->alternate_scan = op.pictureParameters().picture_coding_extension.bits.alternate_scan;
    c->secondfield = op.pictureParameters().picture_coding_extension.bits.picture_structure != 3 &&
        !op.pictureParameters().picture_coding_extension.bits.is_first_field;
    c->q_scale_type = op.pictureParameters().picture_coding_extension.bits.q_scale_type;
    c->top_field_first = op.pictureParameters().picture_coding_extension.bits.top_field_first;

    if (op.iqMatrix().load_intra_quantiser_matrix)
        memcpy(c->quant_mat_8x8intra, op.iqMatrix().intra_quantiser_matrix, 64);
    else
        memcpy(c->quant_mat_8x8intra, quant_mat_8x8intra, sizeof(c->quant_mat_8x8intra));
    if (op.iqMatrix().load_non_intra_quantiser_matrix)
        memcpy(c->quant_mat_8x8nonintra, op.iqMatrix().non_intra_quantiser_matrix, 64);
    else
        memcpy(c->quant_mat_8x8nonintra, quant_mat_8x8nonintra, sizeof(c->quant_mat_8x8nonintra));

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

    M(NVC5B0_SET_APPLICATION_ID, NVC5B0_SET_APPLICATION_ID_ID_MPEG12);
    M(NVC5B0_SET_CONTROL_PARAMS,
        NVC5B0_SET_CONTROL_PARAMS_CODEC_TYPE_MPEG2 | NVC5B0_SET_CONTROL_PARAMS_GPTIMER_ON /*| NVC5B0_SET_CONTROL_PARAMS_ERR_CONCEAL_ON*/ | NVC5B0_SET_CONTROL_PARAMS_ERROR_FRM_IDX(0) | NVC5B0_SET_CONTROL_PARAMS_MBTIMER_ON);
    M(NVC5B0_SET_DRV_PIC_SETUP_OFFSET, 0xdeadbeef);
    BO(&_config_bo, 0, false);
    M(NVC5B0_SET_IN_BUF_BASE_OFFSET, 0xdeadbeef);
    BO(op.sliceData(), 0, false);
    M(NVC5B0_SET_SLICE_OFFSETS_BUF_OFFSET, 0xdeadbeef);
    BO(op.sliceDataOffsets(), 0, false);
    static unsigned int pidx = 0;
    M(NVC5B0_SET_PICTURE_INDEX, pidx++);
    M(NVC5B0_SET_PICTURE_LUMA_OFFSET0, 0xdeadbeef);
    BO(op.output().bo, 0, true);
    M(NVC5B0_SET_PICTURE_CHROMA_OFFSET0, 0xdeadbeef);
    BO(op.output().bo, op.output().pitch * op.output().paddedHeight(), true);
    static int picture_index = 0;
    picture_index++;
    if (op.forwardReference()) {
        M(NVC5B0_SET_PICTURE_LUMA_OFFSET1, 0xdeadbeef);
        BO(op.forwardReference(), 0, true);
        M(NVC5B0_SET_PICTURE_CHROMA_OFFSET1, 0xdeadbeef);
        BO(op.forwardReference(), op.output().pitch * op.output().paddedHeight(), true);
    } else if (op.backwardReference()) {
        M(NVC5B0_SET_PICTURE_LUMA_OFFSET1, 0xdeadbeef);
        BO(op.backwardReference(), 0, true);
        M(NVC5B0_SET_PICTURE_CHROMA_OFFSET1, 0xdeadbeef);
        BO(op.backwardReference(), op.output().pitch * op.output().paddedHeight(), true);
    } else {
        M(NVC5B0_SET_PICTURE_LUMA_OFFSET1, 0xdeadbeef);
        BO(op.output().bo, 0, true);
        M(NVC5B0_SET_PICTURE_CHROMA_OFFSET1, 0xdeadbeef);
        BO(op.output().bo, op.output().pitch * op.output().paddedHeight(), true);
    }
    if (op.backwardReference()) {
        M(NVC5B0_SET_PICTURE_LUMA_OFFSET2, 0xdeadbeef);
        BO(op.backwardReference(), 0, true);
        M(NVC5B0_SET_PICTURE_CHROMA_OFFSET2, 0xdeadbeef);
        BO(op.backwardReference(), op.output().pitch * op.output().paddedHeight(), true);
    } else if (op.forwardReference() && false) {
        M(NVC5B0_SET_PICTURE_LUMA_OFFSET2, 0xdeadbeef);
        BO(op.forwardReference(), 0, true);
        M(NVC5B0_SET_PICTURE_CHROMA_OFFSET2, 0xdeadbeef);
        BO(op.forwardReference(), op.output().pitch * op.output().paddedHeight(), true);
    } else {
        M(NVC5B0_SET_PICTURE_LUMA_OFFSET2, 0xdeadbeef);
        BO(op.output().bo, 0, true);
        M(NVC5B0_SET_PICTURE_CHROMA_OFFSET2, 0xdeadbeef);
        BO(op.output().bo, op.output().pitch * op.output().paddedHeight(), true);
    }
    M(NVC5B0_SET_HISTOGRAM_OFFSET, 0xdead1400);
    M(NVC5B0_SET_NVDEC_STATUS_OFFSET, 0xdeadbeef);
    BO(&_status_bo, 0, true);
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
        submit.syncpt_incr.syncpt_fd = _dev.syncpointFd(_syncpt);
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
