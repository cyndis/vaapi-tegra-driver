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

#ifndef VIC_H
#define VIC_H

#include <linux/kernel.h>
#include "../gem.h"

class VicOp {
public:
    struct Surface {
        Surface() : bo(nullptr), x(0), y(0), width(0), height(0), pitch(0)
        { }

        GemBuffer *bo;

        // All in pixels
        unsigned int x, y;
        unsigned int width, height, pitch;
        uint32_t fourcc;
        uint64_t format;

        unsigned int paddedHeight() const {
            return __ALIGN_KERNEL(height, 16);
        }
    };

    VicOp();

    void setOutput(VicOp::Surface surf);
    void setSurface(unsigned int idx, VicOp::Surface surf);
    void setClear(float r, float g, float b);

    const VicOp::Surface &output() const { return _output; }
    const VicOp::Surface &input(unsigned int idx) const { return _inputs[idx]; }
    float clearR() const { return _clear_r; }
    float clearG() const { return _clear_g; }
    float clearB() const { return _clear_b; }

private:
    VicOp::Surface _output;
    VicOp::Surface _inputs[1];

    float _clear_r, _clear_g, _clear_b;
};

class VicDevice {
public:
    VicDevice(DrmDevice &dev);
    ~VicDevice();

    int open();
    int run(VicOp &op);

private:
    DrmDevice &_dev;

    enum Version {
        Vic4_0,
        Vic4_1
    };
    Version _version;

    uint64_t _context;
    uint32_t _syncpt;
    GemBuffer _cmd_bo, _config_bo, _filter_bo;
};

#endif // GEM_H
