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

#include "tegra_drm.h"
#include "host1x_uapi.h"

DrmDevice::DrmDevice()
{
    if (access("/dev/host1x", F_OK) == 0 && !getenv("VAAPI_TEGRA_FORCE_OLDAPI")) {
        printf("Using new api.\n");
        _new_api = true;
        _host_fd = open("/dev/host1x", O_RDWR);
        if (_host_fd == -1) {
            perror("Failed to open Host1x device");
        }
    } else {
        printf("Using old api.\n");
    }

    _fd = open("/dev/dri/card0", O_RDWR);
    if (_fd == -1) {
        perror("Failed to open DRM device");
    }
}

DrmDevice::~DrmDevice()
{
    if (_fd != -1)
        close(_fd);

    if (_host_fd != -1)
        close(_host_fd);
}

int DrmDevice::ioctl(int request, void *ptr)
{
    return ::ioctl(_fd, request, ptr);
}

int DrmDevice::host_ioctl(int request, void *ptr)
{
    return ::ioctl(_host_fd, request, ptr);
}

int DrmDevice::open_channel(uint32_t cl, uint64_t *context) {
    int err;

    if (_new_api) {
        struct drm_tegra_channel_open channel_open_args = {0};
        channel_open_args.host1x_class = cl;

        err = ioctl(DRM_IOCTL_TEGRA_CHANNEL_OPEN, &channel_open_args);
        if (err == -1)
            return -1;

        *context = channel_open_args.channel_ctx;
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
        channel_close_args.channel_ctx = context;

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
        struct host1x_allocate_syncpoint allocate_syncpoint_args = {0};
        struct host1x_syncpoint_info syncpoint_info_args = {0};

        err = host_ioctl(HOST1X_IOCTL_ALLOCATE_SYNCPOINT, &allocate_syncpoint_args);
        if (err == -1)
            return -1;

        err = ::ioctl(allocate_syncpoint_args.fd, HOST1X_IOCTL_SYNCPOINT_INFO,
            &syncpoint_info_args);
        if (err == -1) {
            close(allocate_syncpoint_args.fd);
            return -1;
        }

        _syncpt_fds[syncpoint_info_args.id] = allocate_syncpoint_args.fd;
        *syncpt = syncpoint_info_args.id;
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
    close(_syncpt_fds[syncpt]);
    _syncpt_fds.erase(syncpt);
}

int DrmDevice::waitSyncpoint(uint32_t id, uint32_t threshold) {
    int err;

    if (_new_api) {
        struct host1x_create_fence create_fence_args = { 0 };
        create_fence_args.id = id;
        create_fence_args.threshold = threshold;

        err = host_ioctl(HOST1X_IOCTL_CREATE_FENCE, &create_fence_args);
        if (err == -1) {
            perror("Fence creation failed");
            return err;
        }

        pollfd pfd = { 0 };
        pfd.fd = create_fence_args.fence_fd;
        pfd.events = POLLIN;
        err = poll(&pfd, 1, 2000);
        if (err < 1)
            return err;

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
}

GemBuffer::GemBuffer(DrmDevice &dev)
: _dev(dev), _valid(false), _handle(0), _map(nullptr), _mapping_id(0)
{
}

GemBuffer::~GemBuffer()
{
    if (_mapping_id) {
        struct drm_tegra_channel_unmap channel_unmap_args = { 0 };
        channel_unmap_args.mapping_id = _mapping_id;
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

    if (!_dev.isNewApi() || _mapping_id != 0)
        return 0;

    channel_map_args.channel_ctx = channel_ctx;
    channel_map_args.handle = _handle;
    channel_map_args.flags = readwrite ? DRM_TEGRA_CHANNEL_MAP_READWRITE : 0;

    err = _dev.ioctl(DRM_IOCTL_TEGRA_CHANNEL_MAP, &channel_map_args);
    if (err == -1) {
        perror("GEM channel map failed");
        return err;
    }

    _mapping_id = channel_map_args.mapping_id;

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
