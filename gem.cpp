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

#include <libdrm/tegra_drm.h>

DrmDevice::DrmDevice()
{
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

GemBuffer::GemBuffer(DrmDevice &dev)
: _dev(dev), _valid(false), _handle(0), _map(nullptr)
{
}

GemBuffer::~GemBuffer()
{
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
