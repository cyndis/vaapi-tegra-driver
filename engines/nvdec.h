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

#ifndef NVDEC_H
#define NVDEC_H

#include "../gem.h"
#include <va/va_backend.h>
#include <linux/kernel.h>

enum class NvdecCodec {
    MPEG2,
    H264
};

class NvdecOp {
public:
    struct Surface {
        Surface() : bo(nullptr), width(0), height(0), pitch(0)
        { }

        GemBuffer *bo;

        // All in pixels
        unsigned int width, height, pitch;

        unsigned int paddedHeight() const {
            return __ALIGN_KERNEL(height, 16);
        }
    };

    struct MPEG2 {
        VAPictureParameterBufferMPEG2 picture_parameters;
        VAIQMatrixBufferMPEG2 iq_matrix;
        GemBuffer *forward_reference;
        GemBuffer *backward_reference;
    };

    struct H264 {
        VAPictureParameterBufferH264 picture_parameters;
        VASliceParameterBufferH264 slice_parameters;
        VAIQMatrixBufferH264 iq_matrix;
        GemBuffer *references[16];
    };

    NvdecOp();

    void setCodec(NvdecCodec codec) {
        _codec = codec;
    }

    NvdecCodec codec() const {
        return _codec;
    }

    MPEG2 &mpeg2() {
        return _mpeg2;
    }

    H264 &h264() {
        return _h264;
    }

    void setSliceData(GemBuffer *slice_data) {
        _slice_data = slice_data;
    }
    GemBuffer *sliceData() const {
        return _slice_data;
    }

    void setSliceDataLength(uint32_t slice_data_length) {
        _slice_data_length = slice_data_length;
    }
    uint32_t sliceDataLength() const {
        return _slice_data_length;
    }

    void setNumSlices(uint32_t num_slices) {
        _num_slices = num_slices;
    }
    uint32_t numSlices() const {
        return _num_slices;
    }

    void setSliceDataOffsets(GemBuffer *slice_data_offsets) {
        _slice_data_offsets = slice_data_offsets;
    }
    GemBuffer *sliceDataOffsets() const {
        return _slice_data_offsets;
    }

    void setOutput(NvdecOp::Surface surf) { _output = surf; }
    const Surface &output() const { return _output; }

private:
    NvdecCodec _codec;

    MPEG2 _mpeg2;
    H264 _h264;

    GemBuffer *_slice_data;
    uint32_t _slice_data_length;
    uint32_t _num_slices;
    GemBuffer *_slice_data_offsets;
    NvdecOp::Surface _output;
};

class NvdecDevice {
public:
    NvdecDevice(DrmDevice &dev);
    ~NvdecDevice();

    int open();
    int run(NvdecOp &op);

private:
    DrmDevice &_dev;

    uint64_t _context;
    uint32_t _syncpt;
    GemBuffer _cmd_bo, _config_bo, _status_bo;
    GemBuffer _history_bo, _mbhist_bo, _coloc_bo;
    bool _is210;

    struct SlotManager {
        std::array<VASurfaceID, 17> slots;

        SlotManager();
        void clean(const VAPictureH264 *refs, size_t num_refs);
        int get(VASurfaceID surface, bool insert);
    } _slots;

    void runH264(void *cfg, NvdecOp &op, std::vector<GemBuffer *> &surfaces);
};

#endif // GEM_H
