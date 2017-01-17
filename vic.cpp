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

#include "vic.h"
#include "vic04.h"
#include "host1x.h"
#include <libdrm/drm.h>
#include <libdrm/tegra_drm.h>

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
: _dev(dev), _cmd_bo(dev), _config_bo(dev)
{
}

VicDevice::~VicDevice() {
    if (_context) {
        struct drm_tegra_close_channel close_channel_args;
        close_channel_args.context = _context;

        _dev.ioctl(DRM_IOCTL_TEGRA_CLOSE_CHANNEL, &close_channel_args);
    }
}

int VicDevice::open() {
    struct drm_tegra_open_channel open_channel_args;
    int err;

    memset(&open_channel_args, 0, sizeof(open_channel_args));
    open_channel_args.client = HOST1X_CLASS_VIC;

    err = _dev.ioctl(DRM_IOCTL_TEGRA_OPEN_CHANNEL, &open_channel_args);
    if (err == -1) {
        perror("Channel open failed");
        return err;
    }

    _context = open_channel_args.context;

    err = _cmd_bo.allocate(128);
    if (err)
        return err;

    err = _config_bo.allocate(sizeof(ConfigStruct));
    if (err)
        return err;

    struct drm_tegra_get_syncpt get_syncpt_args;
    memset(&get_syncpt_args, 0, sizeof(get_syncpt_args));
    get_syncpt_args.context = _context;
    get_syncpt_args.index = 0;

    err = _dev.ioctl(DRM_IOCTL_TEGRA_GET_SYNCPT, &get_syncpt_args);
    if (err == -1) {
        perror("Syncpt get failed");
        return err;
    }

    _syncpt = get_syncpt_args.id;

    return 0;
}

int VicDevice::run(VicOp &op)
{
    int err, i;
    ConfigStruct *c = (ConfigStruct *)_config_bo.map();
    uint32_t *cmd = (uint32_t *)_cmd_bo.map();
    std::vector<drm_tegra_reloc> relocs;

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

    c->outputSurfaceConfig.OutPixelFormat = VIC_PIXEL_FORMAT_A8R8G8B8;
    c->outputSurfaceConfig.OutBlkKind = VIC_BLK_KIND_GENERIC_16Bx2;
    c->outputSurfaceConfig.OutBlkHeight = 4;
    c->outputSurfaceConfig.OutSurfaceWidth = op.output().width-1;
    c->outputSurfaceConfig.OutSurfaceHeight = op.output().height-1;
    c->outputSurfaceConfig.OutLumaWidth = op.output().pitch-1;
    c->outputSurfaceConfig.OutLumaHeight = op.output().height-1;
    c->outputSurfaceConfig.OutChromaWidth = 16383;
    c->outputSurfaceConfig.OutChromaHeight = 16383;

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
        surf.SlotPixelFormat = VIC_PIXEL_FORMAT_Y8_V8U8_N420;
        surf.SlotBlkKind = VIC_BLK_KIND_PITCH;
        surf.SlotCacheWidth = VIC_CACHE_WIDTH_64Bx4;
        surf.SlotSurfaceWidth = in0.width - 1;
        surf.SlotSurfaceHeight = in0.height - 1;
        surf.SlotLumaWidth = in0.pitch - 1;
        surf.SlotLumaHeight = in0.height - 1;
        surf.SlotChromaWidth = (in0.pitch / 2) - 1;
        surf.SlotChromaHeight = (in0.height / 2) - 1;

        surf.SlotChromaLocHoriz = 1;
        surf.SlotChromaLocVert = 1;

        matrixToFixed(rec601_to_rgb, &c->slotStruct[0].colorMatrixStruct);
    }

    memset(cmd, 0, 128);
    i = 0;

#define M(name, value) do {\
    cmd[i++] = host1x_opcode_incr(VIC_UCLASS_METHOD_OFFSET, 2);\
    cmd[i++] = (name) >> 2;\
    cmd[i++] = (value);\
} while (0);
#define BO(h, offs) do {\
    drm_tegra_reloc __reloc;\
    __reloc.cmdbuf.handle = _cmd_bo.handle();\
    __reloc.cmdbuf.offset = (i-1)*4;\
    __reloc.target.handle = (h)->handle();\
    __reloc.target.offset = (offs);\
    __reloc.shift = 8;\
    __reloc.pad = 0;\
    relocs.push_back(__reloc);\
} while(0);

    M(NVB0B6_VIDEO_COMPOSITOR_SET_CONTROL_PARAMS, (sizeof(*c) / 16) << 16);
    M(NVB0B6_VIDEO_COMPOSITOR_SET_CONFIG_STRUCT_OFFSET, 0xdeadbeef);
    BO(&_config_bo, 0);
    M(NVB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_LUMA_OFFSET, 0xdeadbeef);
    BO(op.output().bo, 0);

    if (in0.bo) {
        M(NVB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_LUMA_OFFSET, 0xdeadbeef);
        BO(in0.bo, 0);

        M(NVB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_SLOT0_CHROMA_U_OFFSET, 0xdeadbeef);
        BO(in0.bo, in0.pitch * in0.height);
    }

    M(NVB0B6_VIDEO_COMPOSITOR_EXECUTE, (1 << 8));
    cmd[i++] = host1x_opcode_nonincr(0, 1);
    cmd[i++] = _syncpt | (1 << 8);

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

    struct drm_tegra_syncpt_wait syncpt_wait_args;
    syncpt_wait_args.id = _syncpt;
    syncpt_wait_args.thresh = submit.fence;
    syncpt_wait_args.timeout = 2000;

    err = _dev.ioctl(DRM_IOCTL_TEGRA_SYNCPT_WAIT, &syncpt_wait_args);
    if (err == -1) {
        perror("Syncpt wait failed");
        return err;
    }

    return 0;
}
