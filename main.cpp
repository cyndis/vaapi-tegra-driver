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

#include <cstdio>
#include <cstdint>
#include <cstring>

#include <va/va_backend.h>

#include "objects.h"
#include "gem.h"
#include "vic.h"

extern "C" {
    #include <va/va_dricommon.h>
}

#define FUNC(name, ...) \
    extern "C" VAStatus tegra_ ## name (VADriverContextP ctx, ##__VA_ARGS__)

#define DRIVER_DATA ((DriverData *)ctx->pDriverData)

struct DriverData {
    Objects objects;
    DrmDevice *drm;
    VicDevice *vic;
};

FUNC(Terminate)
{
    DRIVER_DATA->objects.clear();
    delete DRIVER_DATA->vic;
    delete DRIVER_DATA->drm;

    delete DRIVER_DATA;

    ctx->pDriverData = nullptr;

    return VA_STATUS_SUCCESS;
}

FUNC(QueryConfigProfiles, VAProfile *profile_list, int *num_profiles)
{
    *num_profiles = 0;

    return VA_STATUS_SUCCESS;
}

FUNC(QueryConfigEntrypoints, VAProfile profile, VAEntrypoint *entrypoint_list,
     int *num_entrypoints)
{
    *num_entrypoints = 0;

    return VA_STATUS_SUCCESS;
}

FUNC(GetConfigAttributes, VAProfile profile, VAEntrypoint entrypoint,
     VAConfigAttrib *attrib_list, int num_attribs)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(CreateConfig, VAProfile profile, VAEntrypoint entrypoint,
     VAConfigAttrib *attrib_list, int num_attribs, VAConfigID *config_id)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(DestroyConfig, VAConfigID config_id)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(QueryConfigAttributes, VAConfigID config_id, VAProfile *profile,
     VAEntrypoint *entrypoint, VAConfigAttrib *attrib_list, int *num_attribs)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

/* TODO destroy already created surfaces on error */
FUNC(CreateSurfaces, int width, int height, int format, int num_surfaces,
     VASurfaceID *surfaces)
{
    int i, err;

    for (i = 0; i < num_surfaces; ++i) {
        int pitch = 512;

        Surface *surface = DRIVER_DATA->objects.createSurface(&surfaces[i]);
        surface->width = width;
        surface->height = height;
        surface->pitch = pitch;

        size_t size = (pitch * height) + 2 * ((pitch/2) * (height)/2);

        auto gem = std::make_unique<GemBuffer>(*DRIVER_DATA->drm);
        err = gem->allocate(size);
        if (err)
            return VA_STATUS_ERROR_ALLOCATION_FAILED;

        Buffer *buffer = DRIVER_DATA->objects.createBuffer(&surface->buffer);
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

FUNC(DestroySurfaces, VASurfaceID *surface_list, int num_surfaces)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(CreateContext, VAConfigID config_id, int picture_width, int picture_height,
     int flag, VASurfaceID *render_targets, int num_render_targets,
     VAContextID *context)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(DestroyContext, VAContextID context)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(CreateBuffer, VAContextID context, VABufferType type, unsigned int size,
     unsigned int num_elements, void *data, VABufferID *buf_id)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(BufferSetNumElements, VABufferID buf_id, unsigned int num_elements)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(MapBuffer, VABufferID buf_id, void **pbuf)
{
    Buffer *buffer = DRIVER_DATA->objects.buffer(buf_id);

    *pbuf = buffer->gem->map();
    if (*pbuf)
        return VA_STATUS_SUCCESS;
    else
        return VA_STATUS_ERROR_OPERATION_FAILED;
}

FUNC(UnmapBuffer, VABufferID buf_id)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(DestroyBuffer, VABufferID buffer_id)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(BeginPicture, VAContextID context, VASurfaceID render_target)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(RenderPicture, VAContextID context, VABufferID *buffers, int num_buffers)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(EndPicture, VAContextID context)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(SyncSurface, VASurfaceID render_target)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(QuerySurfaceStatus, VASurfaceID render_target, VASurfaceStatus *status)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(QuerySurfaceError, VASurfaceID render_target, VAStatus error_status,
     void **error_info)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(PutSurface, VASurfaceID surface, void* draw, short srcx, short srcy,
     unsigned short srcw, unsigned short srch, short destx, short desty,
     unsigned short destw, unsigned short desth, VARectangle *cliprects,
     unsigned int number_cliprects, unsigned int flags)
{
    struct dri_drawable *dri_drawable;
    union dri_buffer *dri_buffer;

    DrmDevice &device = *DRIVER_DATA->drm;

    dri_drawable = dri_get_drawable(ctx, (Drawable)draw);
    dri_buffer = dri_get_rendering_buffer(ctx, dri_drawable);

    GemBuffer buffer(device);
    buffer.openByName(dri_buffer->dri2.name);

    VicDevice vic(device);
    if (vic.open())
        return VA_STATUS_ERROR_OPERATION_FAILED;

    VicOp op;

    VicOp::Surface op_out;
    op_out.bo = &buffer;
    op_out.width = destw;
    op_out.height = desth;
    op_out.pitch = dri_buffer->dri2.pitch / 4;

    Surface *sf = DRIVER_DATA->objects.surface(surface);
    VicOp::Surface op_in;
    op_in.bo = DRIVER_DATA->objects.buffer(sf->buffer)->gem.get();
    op_in.x = srcx;
    op_in.y = srcy;
    op_in.width = srcw;
    op_in.height = srch;
    op_in.pitch = sf->pitch;

    op.setClear(0.0, 1.0, 0.0);
    op.setOutput(op_out);
    op.setSurface(0, op_in);

    vic.run(op);

    dri_swap_buffer(ctx, dri_drawable);

    return VA_STATUS_SUCCESS;
}

FUNC(QueryImageFormats, VAImageFormat *format_list, int *num_formats)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(CreateImage, VAImageFormat *format, int width, int height, VAImage *image)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(DeriveImage, VASurfaceID surface, VAImage *image)
{
    Surface *s = DRIVER_DATA->objects.surface(surface);

    image->format.fourcc = s->format;
    image->buf = s->buffer;

    image->width = s->width;
    image->height = s->height;
    image->data_size = DRIVER_DATA->objects.buffer(s->buffer)->gem->size();
    image->num_planes = 2;

    image->pitches[0] = s->pitch;
    image->pitches[1] = s->pitch;
    image->pitches[2] = s->pitch;

    image->offsets[0] = 0;
    image->offsets[1] = s->pitch * s->height;
    image->offsets[2] = s->pitch * s->height + 1;

    image->num_palette_entries = 0;
    image->entry_bytes = 0;

    return VA_STATUS_SUCCESS;
}

FUNC(DestroyImage, VAImageID image)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(SetImagePalette, VAImageID image, unsigned char *palette)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(GetImage, VASurfaceID surface, int x, int y, unsigned int width,
     unsigned int height, VAImageID image)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(PutImage, VASurfaceID surface, VAImageID image, int src_x, int src_y,
     unsigned int src_width, unsigned int src_height, int dest_x, int dest_y,
     unsigned int dest_width, unsigned int dest_height)
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

FUNC(SetSubpictureChromakey, VASubpictureID subpicture,
     unsigned int chromakey_min, unsigned int chromakey_max,
     unsigned int chromakey_mask)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(SetSubpictureGlobalAlpha, VASubpictureID subpicture, float global_alpha)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(AssociateSubpicture, VASubpictureID subpicture,
     VASurfaceID *target_surfaces, int num_surfaces, short src_x, short src_y,
     unsigned short src_width, unsigned short src_height, short dest_x,
     short dest_y, unsigned short dest_width, unsigned short dest_height,
     unsigned int flags)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(DeassociateSubpicture, VASubpictureID subpicture,
     VASurfaceID *target_surfaces, int num_surfaces)
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


FUNC(LockSurface, VASurfaceID surface, unsigned int *fourcc,
     unsigned int *luma_stride, unsigned int *chroma_u_stride,
     unsigned int *chroma_v_stride, unsigned int *luma_offset,
     unsigned int *chroma_u_offset, unsigned int *chroma_v_offset,
     unsigned int *buffer_name, void **buffer)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

FUNC(UnlockSurface, VASurfaceID surface)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

extern "C" VAStatus __vaDriverInit_0_39(VADriverContextP ctx)
{
    struct VADriverVTable * const vtbl = ctx->vtable;

    ctx->version_major = 0;
    ctx->version_minor = 1;
    ctx->max_profiles = 1;
    ctx->max_entrypoints = 1;
    ctx->max_attributes = 1;
    ctx->max_image_formats = 1;
    ctx->max_subpic_formats = 1;
    ctx->max_display_attributes = 1;
    ctx->str_vendor = "Tegra Host1x driver";

    DriverData *dd = new DriverData;
    dd->drm = new DrmDevice;
    dd->vic = new VicDevice(*dd->drm);

    ctx->pDriverData = (void *)dd;

    vtbl->vaTerminate = tegra_Terminate;
    vtbl->vaQueryConfigProfiles = tegra_QueryConfigProfiles;
    vtbl->vaQueryConfigEntrypoints = tegra_QueryConfigEntrypoints;
    vtbl->vaGetConfigAttributes = tegra_GetConfigAttributes;
    vtbl->vaCreateConfig = tegra_CreateConfig;
    vtbl->vaDestroyConfig = tegra_DestroyConfig;
    vtbl->vaQueryConfigAttributes = tegra_QueryConfigAttributes;
    vtbl->vaCreateSurfaces = tegra_CreateSurfaces;
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

    return VA_STATUS_SUCCESS;
}
