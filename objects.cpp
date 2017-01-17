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

#include "objects.h"

#include <cstdio>

void Objects::clear()
{
    for (Object *obj : _objects)
        delete obj;

    _objects.clear();
}

Surface * Objects::createSurface(VASurfaceID* id)
{
    Surface *surface = new Surface;

    *id = addGeneric(surface);

    return surface;
}

Surface * Objects::surface(VASurfaceID id)
{
    return static_cast<Surface *>(getGeneric(id));
}

Buffer* Objects::createBuffer(VABufferID* id)
{
    Buffer *buffer = new Buffer;

    *id = addGeneric(buffer);

    return buffer;
}

Buffer * Objects::buffer(VABufferID id)
{
    return static_cast<Buffer *>(getGeneric(id));
}

VAGenericID Objects::addGeneric(Object* obj)
{
    std::lock_guard<std::mutex> g(_lock);
    VAGenericID id;

    _objects.push_back(obj);
    id = _objects.size();

    return id;
}

Object * Objects::getGeneric(VAGenericID id)
{
    std::lock_guard<std::mutex> g(_lock);

    Object *obj = _objects.at(id-1);

    return obj;
}
