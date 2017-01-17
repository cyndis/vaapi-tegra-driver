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

#ifndef OBJECTS_H
#define OBJECTS_H

#include <mutex>
#include <vector>
#include <memory>

#include <va/va_backend.h>

#include "gem.h"

class Object
{
};

class Surface : public Object
{
public:
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    int format;
    VABufferID buffer;
};

class Buffer : public Object
{
public:
    std::unique_ptr<GemBuffer> gem;
};

class Objects
{
public:
    void clear();

    Surface *createSurface(VASurfaceID *id);
    Surface *surface(VASurfaceID id);

    Buffer *createBuffer(VABufferID *id);
    Buffer *buffer(VABufferID id);

private:
    std::mutex _lock;
    std::vector<Object *> _objects;

    VAGenericID addGeneric(Object *obj);
    Object *getGeneric(VAGenericID id);
};

#endif // OBJECTS_H
