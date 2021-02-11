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

#ifndef GEM_H
#define GEM_H

#include <cstdint>
#include <map>

#include <libdrm/drm.h>

class DrmDevice {
public:
    DrmDevice();
    DrmDevice(const DrmDevice &) = delete;
    ~DrmDevice();

    int ioctl(int request, void *ptr);
    int host_ioctl(int request, void *ptr);

    int fd() const { return _fd; }

    int open_channel(uint32_t cl, uint64_t *context);
    int close_channel(uint64_t context);
    int allocate_syncpoint(uint64_t context, uint32_t *syncpt);
    void free_syncpoint(uint32_t syncpt);

    bool isNewApi() const { return _new_api; }
    int syncpointFd(uint32_t id) { return _syncpt_fds[id]; }

    int waitSyncpoint(uint32_t id, uint32_t threshold);

private:
    int _fd, _host_fd;
    bool _new_api;

    std::map<uint32_t, int> _syncpt_fds;
};

typedef uint32_t gem_handle;

class GemBuffer {
public:
    GemBuffer(DrmDevice &dev);
    GemBuffer(const GemBuffer &) = delete;
    ~GemBuffer();

    int allocate(size_t bytes);
    int openByName(uint32_t name);
    void *map();
    int channelMap(uint32_t channel_ctx, bool readwrite);
    int32_t exportFd(bool readwrite);

    gem_handle handle() const { return _handle; }
    size_t size() const { return _size; }
    uint32_t mappingId(uint32_t channel_ctx) const { return _mapping_ids.at(channel_ctx); }

private:
    DrmDevice &_dev;

    bool _valid;
    gem_handle _handle;
    size_t _size;

    void *_map;

    std::map<uint32_t, uint32_t> _mapping_ids;
};

#endif // GEM_H
