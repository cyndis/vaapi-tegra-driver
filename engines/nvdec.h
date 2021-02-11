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

    NvdecOp();

    void setPictureParameters(VAPictureParameterBufferMPEG2 picture_parameters) {
        _picture_parameters = picture_parameters;
    }
    const VAPictureParameterBufferMPEG2 &pictureParameters() const {
        return _picture_parameters;
    }

    void setIQMatrix(VAIQMatrixBufferMPEG2 iq_matrix) {
        _iq_matrix = iq_matrix;
    }
    const VAIQMatrixBufferMPEG2 &iqMatrix() const {
        return _iq_matrix;
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

    void setForwardReference(GemBuffer *forward_reference) {
        _forward_reference = forward_reference;
    }
    GemBuffer *forwardReference() const {
        return _forward_reference;
    }

    void setBackwardReference(GemBuffer *backward_reference) {
        _backward_reference = backward_reference;
    }
    GemBuffer *backwardReference() const {
        return _backward_reference;
    }

    void setOutput(NvdecOp::Surface surf) { _output = surf; }
    const Surface &output() const { return _output; }

private:
    VAPictureParameterBufferMPEG2 _picture_parameters;
    VAIQMatrixBufferMPEG2 _iq_matrix;
    GemBuffer *_slice_data;
    uint32_t _slice_data_length;
    uint32_t _num_slices;
    GemBuffer *_slice_data_offsets;
    GemBuffer *_forward_reference;
    GemBuffer *_backward_reference;
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
    bool _is210;
};

#endif // GEM_H
