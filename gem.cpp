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

#include "gem.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstdio>
#include <cstring>
#include <cstdio>
#include <poll.h>
#include <stdlib.h>
#include <ctime>

#include "uapi_headers/tegra_drm.h"
#include "uapi_headers/host1x_uapi.h"

DrmDevice::DrmDevice()
{
    _new_api = true;

    _fd = open("/dev/dri/card0", O_RDWR);
    if (_fd == -1) {
        perror("Failed to open DRM device");
    }
}

DrmDevice::~DrmDevice()
{
    if (_fd != -1)
        close(_fd);
}

int DrmDevice::ioctl(int request, void *ptr)
{
    return ::ioctl(_fd, request, ptr);
}

int DrmDevice::open_channel(uint32_t cl, uint64_t *context) {
    int err;

    if (_new_api) {
        struct drm_tegra_channel_open channel_open_args = {0};
        channel_open_args.host1x_class = cl;

        err = ioctl(DRM_IOCTL_TEGRA_CHANNEL_OPEN, &channel_open_args);
        if (err == -1)
            return -1;

        *context = channel_open_args.context;
    } else {
        struct drm_tegra_open_channel open_channel_args = {0};
        open_channel_args.client = cl;

        err = ioctl(DRM_IOCTL_TEGRA_OPEN_CHANNEL, &open_channel_args);
        if (err == -1)
            return -1;

        *context = open_channel_args.context;
    }

    return 0;
}

int DrmDevice::close_channel(uint64_t context) {
    if (_new_api) {
        struct drm_tegra_channel_close channel_close_args = {0};
        channel_close_args.context = context;

        return ioctl(DRM_IOCTL_TEGRA_CHANNEL_CLOSE, &channel_close_args);
    } else {
        struct drm_tegra_close_channel close_channel_args = {0};
        close_channel_args.context = context;

        return ioctl(DRM_IOCTL_TEGRA_CLOSE_CHANNEL, &close_channel_args);
    }
}

int DrmDevice::allocate_syncpoint(uint64_t context, uint32_t *syncpt) {
    int err;

    if (_new_api) {
        struct drm_tegra_syncpoint_allocate syncpoint_allocate_args = {0};

        err = ioctl(DRM_IOCTL_TEGRA_SYNCPOINT_ALLOCATE, &syncpoint_allocate_args);
        if (err == -1)
            return -1;

        *syncpt = syncpoint_allocate_args.id;
    } else {
        struct drm_tegra_get_syncpt get_syncpt_args = {0};
        get_syncpt_args.context = context;
        get_syncpt_args.index = 0;

        err = ioctl(DRM_IOCTL_TEGRA_GET_SYNCPT, &get_syncpt_args);
        if (err == -1)
            return -1;

        *syncpt = get_syncpt_args.id;
    }

    return 0;
}

void DrmDevice::free_syncpoint(uint32_t syncpt) {
    struct drm_tegra_syncpoint_free syncpoint_free_args = {0};

    syncpoint_free_args.id = syncpt;

    ioctl(DRM_IOCTL_TEGRA_SYNCPOINT_FREE, &syncpoint_free_args);
}

int DrmDevice::waitSyncpoint(uint32_t id, uint32_t threshold) {
    int err;

    if (_new_api) {
        struct timespec ts;
        struct drm_tegra_syncpoint_wait syncpoint_wait_args = { 0 };

        clock_gettime(CLOCK_MONOTONIC, &ts);

        syncpoint_wait_args.id = id;
        syncpoint_wait_args.threshold = threshold;
        syncpoint_wait_args.timeout_ns = (int64_t)ts.tv_sec * 1000000000 + (int64_t)ts.tv_nsec +
            2000000000;

        err = ioctl(DRM_IOCTL_TEGRA_SYNCPOINT_WAIT, &syncpoint_wait_args);
        if (err == -1) {
            perror("Syncpt wait failed");
            return err;
        }

        return 0;
    } else {
        struct drm_tegra_syncpt_wait syncpt_wait_args = { 0 };
        syncpt_wait_args.id = id;
        syncpt_wait_args.thresh = threshold;
        syncpt_wait_args.timeout = 2000;

        err = ioctl(DRM_IOCTL_TEGRA_SYNCPT_WAIT, &syncpt_wait_args);
        if (err == -1) {
            perror("Syncpt wait failed");
            return err;
        }

        return 0;
    }

    // printf("Syncpoint wait %u:%u timed out\n", id, threshold);
}

GemBuffer::GemBuffer(DrmDevice &dev)
: _dev(dev), _valid(false), _handle(0), _map(nullptr)
{
}

GemBuffer::~GemBuffer()
{
    for (const auto& [context, mapping] : _mapping_ids) {
        struct drm_tegra_channel_unmap channel_unmap_args = { 0 };
        channel_unmap_args.context = context;
        channel_unmap_args.mapping = mapping;
        _dev.ioctl(DRM_IOCTL_TEGRA_CHANNEL_UNMAP, &channel_unmap_args);
    }

    if (_map) {
        munmap(_map, _size);
    }

    if (_valid) {
        struct drm_gem_close close_args;
        memset(&close_args, 0, sizeof(close_args));

        close_args.handle = _handle;

        _dev.ioctl(DRM_IOCTL_GEM_CLOSE, &close_args);
    }
}

int GemBuffer::channelMap(uint32_t channel_ctx, bool readwrite)
{
    struct drm_tegra_channel_map channel_map_args = { 0 };
    int err;

    if (!_dev.isNewApi() || _mapping_ids.find(channel_ctx) != _mapping_ids.end())
        return 0;

    channel_map_args.context = channel_ctx;
    channel_map_args.handle = _handle;
    channel_map_args.flags = readwrite ? DRM_TEGRA_CHANNEL_MAP_READ_WRITE : DRM_TEGRA_CHANNEL_MAP_READ;

    err = _dev.ioctl(DRM_IOCTL_TEGRA_CHANNEL_MAP, &channel_map_args);
    if (err == -1) {
        perror("GEM channel map failed");
        return err;
    }

    _mapping_ids.insert({channel_ctx, channel_map_args.mapping});

    return 0;
}

int GemBuffer::allocate(size_t bytes)
{
    struct drm_tegra_gem_create gem_create_args;
    int err;

    memset(&gem_create_args, 0, sizeof(gem_create_args));
    gem_create_args.size = bytes;

    err = _dev.ioctl(DRM_IOCTL_TEGRA_GEM_CREATE, &gem_create_args);
    if (err == -1) {
        perror("GEM create failed");
        return err;
    }

    _handle = gem_create_args.handle;
    _size = bytes;
    _valid = true;

    return 0;
}

int GemBuffer::openByName(uint32_t name)
{
    struct drm_gem_open open_args;
    int err;

    memset(&open_args, 0, sizeof(open_args));
    open_args.name = name;
    err = _dev.ioctl(DRM_IOCTL_GEM_OPEN, &open_args);
    if (err == -1) {
        perror("GEM open failed");
        return err;
    }

    _handle = open_args.handle;
    _size = open_args.size;

    _valid = true;

    return 0;
}

void * GemBuffer::map()
{
    int err;

    if (!_valid)
        return nullptr;
    if (_map)
        return _map;

    struct drm_tegra_gem_mmap gem_mmap_args;
    memset(&gem_mmap_args, 0, sizeof(gem_mmap_args));
    gem_mmap_args.handle = _handle;

    err = _dev.ioctl(DRM_IOCTL_TEGRA_GEM_MMAP, &gem_mmap_args);
    if (err == -1) {
        perror("GEM mmap failed");
        return nullptr;
    }

    _map = mmap(0, _size, PROT_READ | PROT_WRITE, MAP_SHARED,
                _dev.fd(), gem_mmap_args.offset);
    if (_map == MAP_FAILED) {
        _map = nullptr;
        perror("mmap failed");
        return nullptr;
    }

    return _map;
}

int32_t GemBuffer::exportFd(bool readwrite)
{
    struct drm_prime_handle args;
    int err;

    memset(&args, 0, sizeof(args));

    args.handle = _handle;
    args.flags = readwrite ? DRM_RDWR : 0;

    err = _dev.ioctl(DRM_IOCTL_PRIME_HANDLE_TO_FD, &args);
    if (err == -1) {
        perror("GEM export failed");
        return -1;
    }

    return args.fd;
}
