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

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <libdrm/drm_fourcc.h>
#include <linux/kernel.h>
#include <va/va_backend.h>

#include "buffer.h"
#include "context.h"
#include "gem.h"
#include "objects.h"

#include "engines/vic.h"

extern "C" {
#include <va/va_dricommon.h>
}

#define FUNC(name, ...) extern "C" VAStatus tegra_##name(VADriverContextP ctx, ##__VA_ARGS__)

#define DRIVER_DATA ((DriverData *)ctx->pDriverData)

struct DriverData {
    Objects objects;
    DrmDevice *drm;
    VicDevice *vic;
    NvdecDevice *nvdec;
};

FUNC(Terminate)
{
    DRIVER_DATA->objects.clear();
    delete DRIVER_DATA->nvdec;
    delete DRIVER_DATA->vic;
    delete DRIVER_DATA->drm;

    delete DRIVER_DATA;

    ctx->pDriverData = nullptr;

    return VA_STATUS_SUCCESS;
}

FUNC(QueryConfigProfiles, VAProfile *profile_list, int *num_profiles)
{
    int num = 0;

    profile_list[num++] = VAProfileMPEG2Main;
    profile_list[num++] = VAProfileH264ConstrainedBaseline;
    profile_list[num++] = VAProfileH264Main;
    profile_list[num++] = VAProfileH264High;

    *num_profiles = num;

    return VA_STATUS_SUCCESS;
}

FUNC(QueryConfigEntrypoints, VAProfile profile, VAEntrypoint *entrypoint_list, int *num_entrypoints)
{
    switch (profile) {
    case VAProfileMPEG2Main:
    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
        entrypoint_list[0] = VAEntrypointVLD;

        *num_entrypoints = 1;

        break;

    default:
        *num_entrypoints = 0;
    }

    return VA_STATUS_SUCCESS;
}

FUNC(GetConfigAttributes, VAProfile profile, VAEntrypoint entrypoint, VAConfigAttrib *attrib_list,
    int num_attribs)
{
    int i;

    if (profile != VAProfileMPEG2Main && profile != VAProfileH264Main &&
        profile != VAProfileH264ConstrainedBaseline && profile != VAProfileH264High)
        return VA_STATUS_ERROR_INVALID_VALUE;

    if (entrypoint != VAEntrypointVLD)
        return VA_STATUS_ERROR_INVALID_VALUE;

    for (i = 0; i < num_attribs; i++) {
        switch (attrib_list[i].type) {
        case VAConfigAttribRTFormat:
            attrib_list[i].value = VA_RT_FORMAT_YUV420;
            break;
        default:
            attrib_list[i].value = VA_ATTRIB_NOT_SUPPORTED;
        }
    }

    return VA_STATUS_SUCCESS;
}

const VAConfigID CONFIG_MPEG2 = 1001;
const VAConfigID CONFIG_H264 = 1002;

FUNC(CreateConfig, VAProfile profile, VAEntrypoint entrypoint, VAConfigAttrib *attrib_list,
    int num_attribs, VAConfigID *config_id)
{
    if (entrypoint != VAEntrypointVLD)
        return VA_STATUS_ERROR_INVALID_VALUE;

    switch (profile) {
    case VAProfileMPEG2Main:
        *config_id = CONFIG_MPEG2;
        break;
    case VAProfileH264ConstrainedBaseline:
    case VAProfileH264Main:
    case VAProfileH264High:
        *config_id = CONFIG_H264;
        break;
    default:
        return VA_STATUS_ERROR_INVALID_VALUE;
    }

    return VA_STATUS_SUCCESS;
}

FUNC(DestroyConfig, VAConfigID config_id)
{
    return VA_STATUS_SUCCESS;
}

FUNC(QueryConfigAttributes, VAConfigID config_id, VAProfile *profile, VAEntrypoint *entrypoint,
    VAConfigAttrib *attrib_list, int *num_attribs)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

/* TODO destroy already created surfaces on error */
FUNC(CreateSurfaces2, unsigned int format, unsigned int width, unsigned int height,
    VASurfaceID *surfaces, unsigned int num_surfaces, VASurfaceAttrib *attrib_list,
    unsigned int num_attribs)
{
    int i, err;
    unsigned padded_height = __ALIGN_KERNEL(height, 16);

    for (i = 0; i < (int)num_surfaces; ++i) {
        int pitch = __ALIGN_KERNEL(width, 256);

        Surface *surface = DRIVER_DATA->objects.createSurface(&surfaces[i]);
        surface->width = width;
        surface->height = height;
        surface->pitch = pitch;

        size_t size = (pitch * padded_height) + (pitch * padded_height / 2);

        auto gem = std::make_unique<GemBuffer>(*DRIVER_DATA->drm);
        err = gem->allocate(size);
        if (err)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        Buffer *buffer = DRIVER_DATA->objects.createBuffer(&surface->buffer);
        buffer->has_gem = true;
        buffer->type = VABufferTypeMax;
        buffer->gem = std::move(gem);

        switch (format) {
        case VA_RT_FORMAT_YUV420:
            surface->format = VA_FOURCC_NV12;
            break;
        default:
            return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
        }
    }

    return VA_STATUS_SUCCESS;
}

FUNC(CreateSurfaces, int width, int height, int format, int num_surfaces, VASurfaceID *surfaces)
{
    return tegra_CreateSurfaces2(ctx, format, width, height, surfaces, num_surfaces, nullptr, 0);
}

FUNC(ExportSurfaceHandle, VASurfaceID surface_id, uint32_t mem_type, uint32_t flags,
    void *descriptor)
{
    Surface *surface = DRIVER_DATA->objects.surface(surface_id);
    if (!surface)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    Buffer *buffer = DRIVER_DATA->objects.buffer(surface->buffer);

    if (mem_type != VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2)
        return VA_STATUS_ERROR_UNIMPLEMENTED;

    VADRMPRIMESurfaceDescriptor desc;

    desc.fourcc = VA_FOURCC_NV12;
    desc.width = surface->width;
    desc.height = surface->height;

    desc.num_objects = 1;
    desc.objects[0].fd = buffer->gem->exportFd((flags & VA_EXPORT_SURFACE_WRITE_ONLY) != 0);
    if (desc.objects[0].fd == -1)
        return VA_STATUS_ERROR_UNKNOWN;
    desc.objects[0].size = buffer->gem->size();
    desc.objects[0].drm_format_modifier = DRM_FORMAT_NV12;

    desc.num_layers = 2;
    desc.layers[0].drm_format = DRM_FORMAT_R8;
    desc.layers[0].num_planes = 1;
    desc.layers[0].object_index[0] = 0;
    desc.layers[0].offset[0] = 0;
    desc.layers[0].pitch[0] = surface->pitch;
    desc.layers[1].drm_format = DRM_FORMAT_GR88;
    desc.layers[1].num_planes = 1;
    desc.layers[1].object_index[0] = 0;
    desc.layers[1].offset[0] = surface->pitch * __ALIGN_KERNEL(surface->height, 16);
    desc.layers[1].pitch[0] = surface->pitch;

    *(VADRMPRIMESurfaceDescriptor *)descriptor = desc;

    return VA_STATUS_SUCCESS;
}

FUNC(DestroySurfaces, VASurfaceID *surface_list, int num_surfaces)
{
    return VA_STATUS_SUCCESS;
}

FUNC(CreateContext, VAConfigID config_id, int picture_width, int picture_height, int flag,
    VASurfaceID *render_targets, int num_render_targets, VAContextID *context_id)
{
    Context *context = DRIVER_DATA->objects.createContext(context_id);
    if (!context)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    if (config_id == CONFIG_MPEG2)
        context->op.setCodec(NvdecCodec::MPEG2);
    else if (config_id == CONFIG_H264)
        context->op.setCodec(NvdecCodec::H264);

    return VA_STATUS_SUCCESS;
}

FUNC(DestroyContext, VAContextID context)
{
    return VA_STATUS_SUCCESS;
}

FUNC(CreateBuffer, VAContextID context, VABufferType type, unsigned int size,
    unsigned int num_elements, void *data, VABufferID *buf_id)
{
    Buffer *buffer = DRIVER_DATA->objects.createBuffer(buf_id);
    if (!buffer)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    buffer->type = type;
    buffer->has_gem = false;
    buffer->data.resize(size * num_elements);

    if (data)
        memcpy(buffer->data.data(), data, size * num_elements);

    return VA_STATUS_SUCCESS;
}

FUNC(BufferSetNumElements, VABufferID buf_id, unsigned int num_elements)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(MapBuffer, VABufferID buf_id, void **pbuf)
{
    Buffer *buffer = DRIVER_DATA->objects.buffer(buf_id);

    if (buffer->has_gem) {
        *pbuf = buffer->gem->map();
        if (*pbuf)
            return VA_STATUS_SUCCESS;
        else
            return VA_STATUS_ERROR_OPERATION_FAILED;
    } else {
        *pbuf = buffer->data.data();
        return VA_STATUS_SUCCESS;
    }
}

FUNC(UnmapBuffer, VABufferID buf_id)
{
    return VA_STATUS_SUCCESS;
}

FUNC(DestroyBuffer, VABufferID buffer_id)
{
    return VA_STATUS_SUCCESS;
}

FUNC(BeginPicture, VAContextID context_id, VASurfaceID render_target)
{
    Context *context = DRIVER_DATA->objects.context(context_id);
    if (!context)
        return VA_STATUS_ERROR_INVALID_CONTEXT;

    Surface *surface = DRIVER_DATA->objects.surface(render_target);
    if (!surface)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    Buffer *surf_buffer = DRIVER_DATA->objects.buffer(surface->buffer);

    NvdecOp::Surface output_surface;
    output_surface.bo = surf_buffer->gem.get();
    output_surface.width = surface->width;
    output_surface.height = surface->height;
    output_surface.pitch = surface->pitch;

    /* TODO will need temp surface to unswizzle */

    if (context->op.codec() == NvdecCodec::MPEG2)
        context->op.mpeg2() = NvdecOp::MPEG2();
    else if (context->op.codec() == NvdecCodec::H264)
        context->op.h264() = NvdecOp::H264();

    context->op.setOutput(output_surface);
    context->op.setSliceData(nullptr);
    context->op.setSliceDataLength(0);
    context->op.setSliceDataOffsets(nullptr);
    context->num_slices = 0;
    context->total_slice_size = 0;

    return VA_STATUS_SUCCESS;
}

const uint8_t termination_sequence_mpeg2[16] = { 0x00, 0x00, 0x01, 0xB7, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0xB7, 0x00, 0x00, 0x00, 0x00 };

const uint8_t termination_sequence_h264[16] = { 0x00, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x0B, 0x00, 0x00, 0x00, 0x00 };

FUNC(RenderPicture, VAContextID context_id, VABufferID *buffers, int num_buffers)
{
    int i;

    Context *context = DRIVER_DATA->objects.context(context_id);
    if (!context)
        return VA_STATUS_ERROR_INVALID_CONTEXT;

    uint32_t total_slice_size;

    if (context->op.codec() == NvdecCodec::MPEG2)
        total_slice_size = sizeof(termination_sequence_mpeg2);
    else if (context->op.codec() == NvdecCodec::H264)
        total_slice_size = sizeof(termination_sequence_h264);
    else
        return VA_STATUS_ERROR_UNKNOWN;

    uint32_t num_slices = 0;
    for (i = 0; i < num_buffers; i++) {
        Buffer *buffer = DRIVER_DATA->objects.buffer(buffers[i]);
        if (!buffer)
            return VA_STATUS_ERROR_INVALID_BUFFER;

        if (buffer->type != VASliceParameterBufferType)
            continue;

        auto *buff = (VASliceParameterBufferBase *)buffer->data.data();

        total_slice_size += buff->slice_data_size;
        if (context->op.codec() == NvdecCodec::H264)
            total_slice_size += 3;

        if ((buff->slice_data_flag == VA_SLICE_DATA_FLAG_ALL) ||
            (buff->slice_data_flag & VA_SLICE_DATA_FLAG_BEGIN))
            num_slices++;

        if (buff->slice_data_flag != VA_SLICE_DATA_FLAG_ALL) {
            printf("Partial slice!!\n");
        }
    }

    if (num_slices > 0) {
        context->num_slices = num_slices;
        context->total_slice_size = total_slice_size;

        if (!context->slice_data.get() || context->slice_data->size() < total_slice_size) {
            auto slice_size = __ALIGN_KERNEL(total_slice_size, 0x10000);

            context->slice_data.reset();

            auto gem = std::make_unique<GemBuffer>(*DRIVER_DATA->drm);
            int err = gem->allocate(slice_size);
            if (err)
                return VA_STATUS_ERROR_ALLOCATION_FAILED;

            context->slice_data = std::move(gem);
        }

        memset(context->slice_data->map(), 0, context->slice_data->size());

        if (!context->slice_data_offsets.get() ||
            context->slice_data_offsets->size() < num_slices * 4) {
            auto size = __ALIGN_KERNEL((num_slices + 1) * 4, 0x1000);

            context->slice_data_offsets.reset();

            auto gem = std::make_unique<GemBuffer>(*DRIVER_DATA->drm);
            int err = gem->allocate(size);
            if (err)
                return VA_STATUS_ERROR_ALLOCATION_FAILED;

            context->slice_data_offsets = std::move(gem);
        }

        memset(context->slice_data_offsets->map(), 0, context->slice_data_offsets->size());
    }

    uint32_t current_slice_data_offset = 0;
    uint32_t current_slice_idx = 0;
    VASliceParameterBufferBase current_slice_param;

    for (i = 0; i < num_buffers; i++) {
        Buffer *buffer = DRIVER_DATA->objects.buffer(buffers[i]);
        if (!buffer)
            return VA_STATUS_ERROR_INVALID_BUFFER;

        switch (buffer->type) {
        case VAPictureParameterBufferType: {
#define SET_REF_PIC(surface_id, output) \
    if ((surface_id) != VA_INVALID_SURFACE) { \
        Surface *ref = DRIVER_DATA->objects.surface((surface_id)); \
        if (!ref) \
            return VA_STATUS_ERROR_INVALID_SURFACE; \
        Buffer *buf = DRIVER_DATA->objects.buffer(ref->buffer); \
        output = buf->gem.get(); \
    }

            switch (context->op.codec()) {
            case NvdecCodec::MPEG2: {
                auto *buff = (VAPictureParameterBufferMPEG2 *)buffer->data.data();
                context->op.mpeg2().picture_parameters = *buff;

                SET_REF_PIC(buff->forward_reference_picture, context->op.mpeg2().forward_reference);
                SET_REF_PIC(
                    buff->backward_reference_picture, context->op.mpeg2().backward_reference);

                break;
            }
            case NvdecCodec::H264: {
                auto *buff = (VAPictureParameterBufferH264 *)buffer->data.data();
                context->op.h264().picture_parameters = *buff;

                for (size_t i = 0; i < buff->num_ref_frames; i++)
                    SET_REF_PIC(
                        buff->ReferenceFrames[i].picture_id, context->op.h264().references[i]);
            }
            }

            break;
        }
        case VAIQMatrixBufferType:
            switch (context->op.codec()) {
            case NvdecCodec::MPEG2:
                context->op.mpeg2().iq_matrix = *(VAIQMatrixBufferMPEG2 *)buffer->data.data();
                break;
            case NvdecCodec::H264:
                context->op.h264().iq_matrix = *(VAIQMatrixBufferH264 *)buffer->data.data();
                break;
            }

            break;
        case VASliceParameterBufferType: {
            auto *buff = (VASliceParameterBufferBase *)buffer->data.data();
            current_slice_param = *buff;

            if (context->op.codec() == NvdecCodec::H264) {
                auto *h264 = (VASliceParameterBufferH264 *)buffer->data.data();
                context->op.h264().slice_parameters = *h264;
            }

            break;
        }
        case VASliceDataBufferType: {
            uint32_t *offs_ptr = (uint32_t *)context->slice_data_offsets->map();
            *(offs_ptr + current_slice_idx++) = current_slice_data_offset;

            uint8_t *ptr = (uint8_t *)context->slice_data->map();

            if (context->op.codec() == NvdecCodec::H264) {
                memcpy(ptr + current_slice_data_offset, "\x00\x00\x01", 3);
                current_slice_data_offset += 3;
            }

            memcpy(ptr + current_slice_data_offset,
                buffer->data.data() + current_slice_param.slice_data_offset,
                current_slice_param.slice_data_size);

            current_slice_data_offset += current_slice_param.slice_data_size;

            break;
        }
        default:
            printf("WARNING: Trying to use unknown buffer type %u for rendering\n", buffer->type);
            break;
        }
    }

    if (num_slices > 0) {
        uint8_t *ptr = (uint8_t *)context->slice_data->map();

        if (context->op.codec() == NvdecCodec::MPEG2)
            memcpy(ptr + current_slice_data_offset, termination_sequence_mpeg2,
                sizeof(termination_sequence_mpeg2));
        else if (context->op.codec() == NvdecCodec::H264)
            memcpy(ptr + current_slice_data_offset, termination_sequence_h264,
                sizeof(termination_sequence_h264));
        else
            return VA_STATUS_ERROR_UNKNOWN;

        uint32_t *offs_ptr = (uint32_t *)context->slice_data_offsets->map();
        *(offs_ptr + current_slice_idx) = current_slice_data_offset;
    }

    return VA_STATUS_SUCCESS;
}

FUNC(EndPicture, VAContextID context_id)
{
    Context *context = DRIVER_DATA->objects.context(context_id);
    if (!context)
        return VA_STATUS_ERROR_INVALID_CONTEXT;

    if (DRIVER_DATA->nvdec->open())
        return VA_STATUS_ERROR_OPERATION_FAILED;

    context->op.setSliceData(context->slice_data.get());
    context->op.setSliceDataOffsets(context->slice_data_offsets.get());
    context->op.setSliceDataLength(context->total_slice_size);
    context->op.setNumSlices(context->num_slices);

    if (DRIVER_DATA->nvdec->run(context->op))
        return VA_STATUS_ERROR_OPERATION_FAILED;

    return VA_STATUS_SUCCESS;
}

FUNC(SyncSurface, VASurfaceID render_target)
{
    return VA_STATUS_SUCCESS;
}

FUNC(QuerySurfaceStatus, VASurfaceID render_target, VASurfaceStatus *status)
{
    *status = VASurfaceReady;

    return VA_STATUS_SUCCESS;
}

FUNC(QuerySurfaceError, VASurfaceID render_target, VAStatus error_status, void **error_info)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(PutSurface, VASurfaceID surface, void *draw, short srcx, short srcy, unsigned short srcw,
    unsigned short srch, short destx, short desty, unsigned short destw, unsigned short desth,
    VARectangle *cliprects, unsigned int number_cliprects, unsigned int flags)
{
    struct dri_drawable *dri_drawable;
    union dri_buffer *dri_buffer;

    DrmDevice &device = *DRIVER_DATA->drm;

    dri_drawable = va_dri_get_drawable(ctx, (Drawable)draw);
    dri_buffer = va_dri_get_rendering_buffer(ctx, dri_drawable);

    GemBuffer buffer(device);
    buffer.openByName(dri_buffer->dri2.name);

    if (DRIVER_DATA->vic->open())
        return VA_STATUS_ERROR_OPERATION_FAILED;

    VicOp op;

    VicOp::Surface op_out;
    op_out.bo = &buffer;
    op_out.width = destw;
    op_out.height = desth;
    op_out.pitch = dri_buffer->dri2.pitch / 4;
    op_out.fourcc = DRM_FORMAT_ARGB8888;
    op_out.format = DRM_FORMAT_MOD_LINEAR;

    Surface *sf = DRIVER_DATA->objects.surface(surface);
    VicOp::Surface op_in;
    op_in.bo = DRIVER_DATA->objects.buffer(sf->buffer)->gem.get();
    op_in.x = srcx;
    op_in.y = srcy;
    op_in.width = srcw;
    op_in.height = srch;
    op_in.pitch = sf->pitch;
    op_in.fourcc = DRM_FORMAT_NV12;
    op_in.format = DRM_FORMAT_MOD_LINEAR;

    op.setClear(0.0, 1.0, 0.0);
    op.setOutput(op_out);
    op.setSurface(0, op_in);

    DRIVER_DATA->vic->run(op);

    va_dri_swap_buffer(ctx, dri_drawable);

    return VA_STATUS_SUCCESS;
}

FUNC(QueryImageFormats, VAImageFormat *format_list, int *num_formats)
{
    format_list[0].fourcc = VA_FOURCC_NV12;
    format_list[0].byte_order = VA_LSB_FIRST;
    format_list[0].bits_per_pixel = 24;

    *num_formats = 1;

    return VA_STATUS_SUCCESS;
}

FUNC(CreateImage, VAImageFormat *format, int width, int height, VAImage *image_data)
{
    Image *image = DRIVER_DATA->objects.createImage(&image_data->image_id);
    Buffer *buffer = DRIVER_DATA->objects.createBuffer(&image_data->buf);

    image_data->format = *format;
    image_data->width = width;
    image_data->height = height;
    image_data->data_size = width * height * 3;
    image_data->num_planes = 2;
    image_data->pitches[0] = __ALIGN_KERNEL(width, 256);
    image_data->pitches[1] = __ALIGN_KERNEL(width, 256);
    image_data->offsets[0] = 0;
    image_data->offsets[1] = image_data->pitches[0] * __ALIGN_KERNEL(height, 16);
    image_data->num_palette_entries = 0;
    image_data->entry_bytes = 0;

    auto gem = std::make_unique<GemBuffer>(*DRIVER_DATA->drm);
    int err = gem->allocate(image_data->data_size);
    if (err)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    buffer->has_gem = true;
    buffer->gem = std::move(gem);

    image->buffer = buffer;

    return VA_STATUS_SUCCESS;
}

FUNC(DeriveImage, VASurfaceID surface, VAImage *image)
{
    Surface *s = DRIVER_DATA->objects.surface(surface);

    image->format.fourcc = s->format;
    image->buf = s->buffer;
    image->image_id = 0;

    image->width = s->width;
    image->height = s->height;
    image->data_size = DRIVER_DATA->objects.buffer(s->buffer)->gem->size();
    image->num_planes = 2;

    image->pitches[0] = s->pitch;
    image->pitches[1] = s->pitch;
    image->pitches[2] = s->pitch;

    image->offsets[0] = 0;
    image->offsets[1] = s->pitch * __ALIGN_KERNEL(s->height, 16);
    image->offsets[2] = s->pitch * __ALIGN_KERNEL(s->height, 16) + 1;

    image->num_palette_entries = 0;
    image->entry_bytes = 0;

    return VA_STATUS_SUCCESS;
}

FUNC(DestroyImage, VAImageID image_id)
{
    if (image_id == 0)
        return VA_STATUS_SUCCESS;

    Image *image = DRIVER_DATA->objects.image(image_id);

    if (!image)
        return VA_STATUS_ERROR_INVALID_IMAGE;

    delete image->buffer->gem.release();

    return VA_STATUS_SUCCESS;
}

FUNC(SetImagePalette, VAImageID image, unsigned char *palette)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(GetImage, VASurfaceID surface_id, int x, int y, unsigned int width, unsigned int height,
    VAImageID image_id)
{
    Surface *surface = DRIVER_DATA->objects.surface(surface_id);
    if (!surface)
        return VA_STATUS_ERROR_INVALID_SURFACE;

    Image *image = DRIVER_DATA->objects.image(image_id);
    if (!image)
        return VA_STATUS_ERROR_INVALID_IMAGE;

    Buffer *surface_buffer = DRIVER_DATA->objects.buffer(surface->buffer);

    void *surface_map = surface_buffer->gem->map();
    void *image_map = image->buffer->gem->map();
    if (!surface_map || !image_map)
        return VA_STATUS_ERROR_OPERATION_FAILED;

    VicOp op;

    VicOp::Surface op_out;
    op_out.bo = image->buffer->gem.get();
    op_out.width = surface->width;
    op_out.height = surface->height;
    op_out.pitch = surface->pitch;
    op_out.fourcc = DRM_FORMAT_NV12;
    op_out.format = DRM_FORMAT_MOD_LINEAR;
    op.setOutput(op_out);

    VicOp::Surface op_in;
    op_in.bo = surface_buffer->gem.get();
    op_in.width = surface->width;
    op_in.height = surface->height;
    op_in.pitch = surface->pitch;
    op_in.fourcc = DRM_FORMAT_NV12;
    op_in.format = DRM_FORMAT_MOD_NVIDIA_16BX2_BLOCK_TWO_GOB;
    op.setSurface(0, op_in);

    if (DRIVER_DATA->vic->open())
        return VA_STATUS_ERROR_OPERATION_FAILED;

    if (DRIVER_DATA->vic->run(op))
        return VA_STATUS_ERROR_OPERATION_FAILED;

    return VA_STATUS_SUCCESS;
}

FUNC(PutImage, VASurfaceID surface, VAImageID image, int src_x, int src_y, unsigned int src_width,
    unsigned int src_height, int dest_x, int dest_y, unsigned int dest_width,
    unsigned int dest_height)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(QuerySubpictureFormats, VAImageFormat *format_list, unsigned int *flags,
    unsigned int *num_formats)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(CreateSubpicture, VAImageID image, VASubpictureID *subpicture)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(DestroySubpicture, VASubpictureID subpicture)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(SetSubpictureImage, VASubpictureID subpicture, VAImageID image)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(SetSubpictureChromakey, VASubpictureID subpicture, unsigned int chromakey_min,
    unsigned int chromakey_max, unsigned int chromakey_mask)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(SetSubpictureGlobalAlpha, VASubpictureID subpicture, float global_alpha)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(AssociateSubpicture, VASubpictureID subpicture, VASurfaceID *target_surfaces, int num_surfaces,
    short src_x, short src_y, unsigned short src_width, unsigned short src_height, short dest_x,
    short dest_y, unsigned short dest_width, unsigned short dest_height, unsigned int flags)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(DeassociateSubpicture, VASubpictureID subpicture, VASurfaceID *target_surfaces,
    int num_surfaces)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(QueryDisplayAttributes, VADisplayAttribute *attr_list, int *num_attributes)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(GetDisplayAttributes, VADisplayAttribute *attr_list, int num_attributes)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(SetDisplayAttributes, VADisplayAttribute *attr_list, int num_attributes)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(BufferInfo, VABufferID buf_id, VABufferType *type, unsigned int *size,
    unsigned int *num_elements)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(LockSurface, VASurfaceID surface, unsigned int *fourcc, unsigned int *luma_stride,
    unsigned int *chroma_u_stride, unsigned int *chroma_v_stride, unsigned int *luma_offset,
    unsigned int *chroma_u_offset, unsigned int *chroma_v_offset, unsigned int *buffer_name,
    void **buffer)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(UnlockSurface, VASurfaceID surface)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(QuerySurfaceAttributes, VAConfigID config, VASurfaceAttrib *attrib_list,
    unsigned int *num_attribs)
{
    if (attrib_list) {
        attrib_list[0].type = VASurfaceAttribPixelFormat;
        attrib_list[0].value.type = VAGenericValueTypeInteger;
        attrib_list[0].value.value.i = VA_FOURCC_NV12;
        attrib_list[0].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
    }

    *num_attribs = 1;

    return VA_STATUS_SUCCESS;
}

extern "C" VAStatus __vaDriverInit_1_0(VADriverContextP ctx)
{
    struct VADriverVTable *const vtbl = ctx->vtable;

    fprintf(stderr, "\nTegra VIC/NVDEC Driver initializing\n");
    fprintf(stderr, "WARNING: This driver is very experimental!\n");

    ctx->version_major = 0;
    ctx->version_minor = 1;
    ctx->max_profiles = 4;
    ctx->max_entrypoints = 1;
    ctx->max_attributes = 1;
    ctx->max_image_formats = 1;
    ctx->max_subpic_formats = 1;
    ctx->max_display_attributes = 1;
    ctx->str_vendor = "Tegra VIC/NVDEC driver";

    DriverData *dd = new DriverData;
    dd->drm = new DrmDevice;
    dd->vic = new VicDevice(*dd->drm);
    dd->nvdec = new NvdecDevice(*dd->drm);

    ctx->pDriverData = (void *)dd;

    vtbl->vaTerminate = tegra_Terminate;
    vtbl->vaQueryConfigProfiles = tegra_QueryConfigProfiles;
    vtbl->vaQueryConfigEntrypoints = tegra_QueryConfigEntrypoints;
    vtbl->vaGetConfigAttributes = tegra_GetConfigAttributes;
    vtbl->vaCreateConfig = tegra_CreateConfig;
    vtbl->vaDestroyConfig = tegra_DestroyConfig;
    vtbl->vaQueryConfigAttributes = tegra_QueryConfigAttributes;
    vtbl->vaCreateSurfaces = tegra_CreateSurfaces;
    vtbl->vaCreateSurfaces2 = tegra_CreateSurfaces2;
    vtbl->vaExportSurfaceHandle = tegra_ExportSurfaceHandle;
    vtbl->vaDestroySurfaces = tegra_DestroySurfaces;
    vtbl->vaCreateContext = tegra_CreateContext;
    vtbl->vaDestroyContext = tegra_DestroyContext;
    vtbl->vaCreateBuffer = tegra_CreateBuffer;
    vtbl->vaBufferSetNumElements = tegra_BufferSetNumElements;
    vtbl->vaMapBuffer = tegra_MapBuffer;
    vtbl->vaUnmapBuffer = tegra_UnmapBuffer;
    vtbl->vaDestroyBuffer = tegra_DestroyBuffer;
    vtbl->vaBeginPicture = tegra_BeginPicture;
    vtbl->vaRenderPicture = tegra_RenderPicture;
    vtbl->vaEndPicture = tegra_EndPicture;
    vtbl->vaSyncSurface = tegra_SyncSurface;
    vtbl->vaQuerySurfaceStatus = tegra_QuerySurfaceStatus;
    vtbl->vaQuerySurfaceError = tegra_QuerySurfaceError;
    vtbl->vaPutSurface = tegra_PutSurface;
    vtbl->vaQueryImageFormats = tegra_QueryImageFormats;
    vtbl->vaCreateImage = tegra_CreateImage;
    vtbl->vaDeriveImage = tegra_DeriveImage;
    vtbl->vaDestroyImage = tegra_DestroyImage;
    vtbl->vaSetImagePalette = tegra_SetImagePalette;
    vtbl->vaGetImage = tegra_GetImage;
    vtbl->vaPutImage = tegra_PutImage;
    vtbl->vaQuerySubpictureFormats = tegra_QuerySubpictureFormats;
    vtbl->vaCreateSubpicture = tegra_CreateSubpicture;
    vtbl->vaDestroySubpicture = tegra_DestroySubpicture;
    vtbl->vaSetSubpictureImage = tegra_SetSubpictureImage;
    vtbl->vaSetSubpictureChromakey = tegra_SetSubpictureChromakey;
    vtbl->vaSetSubpictureGlobalAlpha = tegra_SetSubpictureGlobalAlpha;
    vtbl->vaAssociateSubpicture = tegra_AssociateSubpicture;
    vtbl->vaDeassociateSubpicture = tegra_DeassociateSubpicture;
    vtbl->vaQueryDisplayAttributes = tegra_QueryDisplayAttributes;
    vtbl->vaGetDisplayAttributes = tegra_GetDisplayAttributes;
    vtbl->vaSetDisplayAttributes = tegra_SetDisplayAttributes;
    vtbl->vaBufferInfo = tegra_BufferInfo;
    vtbl->vaLockSurface = tegra_LockSurface;
    vtbl->vaUnlockSurface = tegra_UnlockSurface;
    vtbl->vaQuerySurfaceAttributes = tegra_QuerySurfaceAttributes;

    return VA_STATUS_SUCCESS;
}
