/* kate: replace-tabs true; indent-width 4
 *
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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

#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cerrno>

#include "vic.h"
#include "../engine_headers/vic04.h"
#include "../engine_headers/host1x.h"
#include <libdrm/drm.h>
#include <libdrm/drm_fourcc.h>
#include "../uapi_headers/tegra_drm.h"
#include <linux/kernel.h>

/*
 * Rec.601 YCbCr to RGB transform matrix. First three columns specify standard
 * rotation/scaling of the YCbCr colorspace to RGB. The last column contains
 * translation of each of the planes, respectively, due to the ranges
 * of YCbCr and RGB being different.
 */
static const float rec601_to_rgb[] = {
    1.16438356f,  0.00000000f,  1.59602679f, -0.87420222f,
    1.16438356f, -0.39176229f, -0.81296765f,  0.53166782f,
    1.16438356f,  2.01723214f,  0.00000000f, -1.08563079f
};

static unsigned int floatToFixed(float v, unsigned int fixed_one) {
    int x = (int)(v * (float)fixed_one + 0.5f);
    if (x < -(1 << 19))
        x = -(1 << 19);
    if (x > ((1 << 19) - 1))
        x = ((1 << 19) - 1);
    return x;
}

static void matrixToFixed(const float in[12], MatrixStruct *out) {
    const unsigned int FIXED_ONE = 0x3ff00;

    float max = in[0];
    for (size_t i = 0; i < 12; ++i) {
        float v = fabsf(in[i]);
        if (v > max)
            max = v;
    }

    int fixed_bits = 0;
    while (fixed_bits < 15 && max < 1024.0f) {
        ++fixed_bits;
        max *= 2.0f;
    }

    out->matrix_coeff00 = floatToFixed(in[0*4+0], 1 << (fixed_bits + 8));
    out->matrix_coeff01 = floatToFixed(in[0*4+1], 1 << (fixed_bits + 8));
    out->matrix_coeff02 = floatToFixed(in[0*4+2], 1 << (fixed_bits + 8));
    out->matrix_coeff03 = floatToFixed(in[0*4+3], FIXED_ONE);

    out->matrix_coeff10 = floatToFixed(in[1*4+0], 1 << (fixed_bits + 8));
    out->matrix_coeff11 = floatToFixed(in[1*4+1], 1 << (fixed_bits + 8));
    out->matrix_coeff12 = floatToFixed(in[1*4+2], 1 << (fixed_bits + 8));
    out->matrix_coeff13 = floatToFixed(in[1*4+3], FIXED_ONE);

    out->matrix_coeff20 = floatToFixed(in[2*4+0], 1 << (fixed_bits + 8));
    out->matrix_coeff21 = floatToFixed(in[2*4+1], 1 << (fixed_bits + 8));
    out->matrix_coeff22 = floatToFixed(in[2*4+2], 1 << (fixed_bits + 8));
    out->matrix_coeff23 = floatToFixed(in[2*4+3], FIXED_ONE);

    out->matrix_r_shift = fixed_bits;
    out->matrix_enable = 1;
}

VicOp::VicOp()
: _clear_r(0.0), _clear_g(0.0), _clear_b(0.0)
{
}

void VicOp::setOutput(VicOp::Surface surf) {
    _output = surf;
}

void VicOp::setSurface(unsigned int idx, VicOp::Surface surf) {
    _inputs[idx] = surf;
}

void VicOp::setClear(float r, float g, float b) {
    _clear_r = r;
    _clear_g = g;
    _clear_b = b;
}

VicDevice::VicDevice(DrmDevice &dev)
: _dev(dev), _context(0), _syncpt(0xffffffff), _cmd_bo(dev), _config_bo(dev), _filter_bo(dev)
{
}

VicDevice::~VicDevice() {
    if (_syncpt != 0xffffffff)
        _dev.free_syncpoint(_syncpt);
    if (_context)
        _dev.close_channel(_context);
}

int VicDevice::open() {
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
        _version = Version::Vic4_0;
    else if (!strcmp(tmp, "24\n"))
        _version = Version::Vic4_1;
    else if (!strcmp(tmp, "25\n"))
        _version = Version::Vic4_1;
    else {
        printf("Unknown chip\n");
        return -1;
    }

    err = _dev.open_channel(HOST1X_CLASS_VIC, &_context);
    if (err == -1) {
        perror("Channel open failed");
        return err;
    }

    err = _cmd_bo.allocate(0x1000);
    if (err)
        return err;

    err = _config_bo.allocate(sizeof(ConfigStruct_VIC41));
    if (err)
        return err;

    err = _config_bo.channelMap(_context, false);
    if (err)
        return err;

    err = _filter_bo.allocate(0x3000);
    if (err)
        return err;

    err = _filter_bo.channelMap(_context, false);
    if (err)
        return err;

    err = _dev.allocate_syncpoint(_context, &_syncpt);
    if (err == -1) {
        perror("Syncpt get failed");
        return err;
    }

    return 0;
}

int VicDevice::run(VicOp &op)
{
    int err, i;
    ConfigStruct_VIC41 *c = (ConfigStruct_VIC41 *)_config_bo.map();
    uint32_t *cmd = (uint32_t *)_cmd_bo.map();
    std::vector<drm_tegra_reloc> relocs;
    std::vector<drm_tegra_submit_buf> relocs_new;
    bool is41 = _version == Version::Vic4_1;

    if (!c || !cmd)
        return 1;

    memset(c, 0, sizeof(*c));

    c->outputConfig.TargetRectTop = 0;
    c->outputConfig.TargetRectLeft = 0;
    c->outputConfig.TargetRectRight = op.output().width-1;
    c->outputConfig.TargetRectBottom = op.output().height-1;
    c->outputConfig.BackgroundAlpha = 1023;
    c->outputConfig.BackgroundR = op.clearR() * 1023;
    c->outputConfig.BackgroundG = op.clearG() * 1023;
    c->outputConfig.BackgroundB = op.clearB() * 1023;

    switch (op.output().fourcc) {
    case DRM_FORMAT_ARGB8888:
        c->outputSurfaceConfig.OutPixelFormat = VIC_PIXEL_FORMAT_A8R8G8B8;
        break;
    case DRM_FORMAT_NV12:
        c->outputSurfaceConfig.OutPixelFormat = VIC_PIXEL_FORMAT_Y8_U8V8_N420;
        break;
    default:
        return 1;
    }
    switch (op.output().format) {
    case DRM_FORMAT_MOD_LINEAR:
        c->outputSurfaceConfig.OutBlkKind = VIC_BLK_KIND_PITCH;
        break;
    default:
        return 1;
    }

    c->outputSurfaceConfig.OutSurfaceWidth = op.output().width-1;
    c->outputSurfaceConfig.OutSurfaceHeight = op.output().height-1;
    c->outputSurfaceConfig.OutLumaWidth = op.output().pitch-1;
    c->outputSurfaceConfig.OutLumaHeight = op.output().height-1;
    c->outputSurfaceConfig.OutChromaWidth = (op.output().pitch / 2)-1;
    c->outputSurfaceConfig.OutChromaHeight = (op.output().height / 2)-1;

    const VicOp::Surface &in0 = op.input(0);
    if (in0.bo) {
        SlotConfig &slot = c->slotStruct[0].slotConfig;
        slot.SlotEnable = 1;
        slot.CurrentFieldEnable = 1;
        slot.PlanarAlpha = 1023;
        slot.ConstantAlpha = 1;
        slot.SourceRectLeft = in0.x << 16;
        slot.SourceRectRight = (in0.width - 1) << 16;
        slot.SourceRectTop = in0.y << 16;
        slot.SourceRectBottom = (in0.height - 1) << 16;
        slot.DestRectLeft = 0;
        slot.DestRectRight = op.output().width - 1;
        slot.DestRectTop = 0;
        slot.DestRectBottom = op.output().height - 1;

        slot.SoftClampHigh = 1023;

        SlotSurfaceConfig &surf = c->slotStruct[0].slotSurfaceConfig;
        switch (in0.fourcc) {
        case DRM_FORMAT_ARGB8888:
            surf.SlotPixelFormat = VIC_PIXEL_FORMAT_A8R8G8B8;
            break;
        case DRM_FORMAT_NV12:
            surf.SlotPixelFormat = VIC_PIXEL_FORMAT_Y8_U8V8_N420;
            if (op.output().fourcc == DRM_FORMAT_ARGB8888)
                matrixToFixed(rec601_to_rgb, &c->slotStruct[0].colorMatrixStruct);
            break;
        default:
            return 1;
        }
        switch (in0.format) {
        case DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB:
            surf.SlotBlkKind = VIC_BLK_KIND_GENERIC_16Bx2;
            surf.SlotBlkHeight = 1;
            surf.SlotCacheWidth = VIC_CACHE_WIDTH_32Bx8;
            break;
        case DRM_FORMAT_MOD_LINEAR:
            surf.SlotBlkKind = VIC_BLK_KIND_PITCH;
            surf.SlotCacheWidth = VIC_CACHE_WIDTH_64Bx4;
            break;
        default:
            return 1;
        }
        surf.SlotSurfaceWidth = in0.width - 1;          // Non-padded width in pixels
        surf.SlotSurfaceHeight = in0.height - 1;        // Height in pixels
        surf.SlotLumaWidth = in0.pitch - 1;             // Padded width in pixels
        surf.SlotLumaHeight = in0.height - 1;           // Height in pixels
        surf.SlotChromaWidth = (in0.pitch / 2) - 1;     // Padded width in pixels
        surf.SlotChromaHeight = (in0.height / 2) - 1;   // Height in pixels

        surf.SlotChromaLocHoriz = 1;
        surf.SlotChromaLocVert = 1;
    }

    memset(cmd, 0, 0x1000);
    i = 0;

#define M(name, value) do {\
    cmd[i++] = host1x_opcode_incr(VIC_UCLASS_METHOD_OFFSET, 2);\
    cmd[i++] = (name) >> 2;\
    cmd[i++] = (value);\
} while (0);
#define BO(h, offs, rw) do {\
    err = (h)->channelMap(_context, (rw));\
    if (err) {\
        perror("Surface mapping failed");\
        return err;\
    }\
    \
    drm_tegra_submit_buf __buf = { 0 };\
    __buf.mapping_id = (h)->mappingId(_context);\
    __buf.reloc.target_offset = (offs);\
    __buf.reloc.gather_offset_words = i-1;\
    __buf.reloc.shift = 8;\
    relocs_new.push_back(__buf);\
    \
    drm_tegra_reloc __reloc;\
    __reloc.cmdbuf.handle = _cmd_bo.handle();\
    __reloc.cmdbuf.offset = (i-1)*4;\
    __reloc.target.handle = (h)->handle();\
    __reloc.target.offset = (offs);\
    __reloc.shift = 8;\
    __reloc.pad = 0;\
    relocs.push_back(__reloc);\
} while(0);

    M(NVB0B6_VIDEO_COMPOSITOR_SET_APPLICATION_ID, 1);
    M(NVB0B6_VIDEO_COMPOSITOR_SET_CONTROL_PARAMS,
        ((is41 ? sizeof(ConfigStruct_VIC41) : sizeof(ConfigStruct_VIC40)) / 16) << 16);
    M(NVB0B6_VIDEO_COMPOSITOR_SET_CONFIG_STRUCT_OFFSET, 0xdeadbeef);
    BO(&_config_bo, 0, false);
    M(NVB0B6_VIDEO_COMPOSITOR_SET_FILTER_STRUCT_OFFSET, 0xdeadbeef);
    BO(&_filter_bo, 0, false);
    M(NVB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_LUMA_OFFSET, 0xdeadbeef);
    BO(op.output().bo, 0, true);
    M(NVB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_CHROMA_U_OFFSET, 0xdeadbeef);
    BO(op.output().bo, op.output().pitch*op.output().paddedHeight(), true);

    if (in0.bo) {
        M(is41 ? NVB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_LUMA_OFFSET
               : NVB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_LUMA_OFFSET,
            0xdeadbeef);
        BO(in0.bo, 0, false);

        M(is41 ? NVB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_CHROMA_U_OFFSET
               : NVB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_CHROMA_U_OFFSET,
               0xdeadbeef);
        BO(in0.bo, in0.pitch * in0.paddedHeight(), false);
    }

    M(NVB0B6_VIDEO_COMPOSITOR_EXECUTE, (1 << 8));
    cmd[i++] = host1x_opcode_nonincr(0, 1);
    cmd[i++] = _syncpt | (1 << (is41 ? 10 : 8));

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
