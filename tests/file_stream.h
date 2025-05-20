// Copyright 2024 Adobe. All rights reserved.
// This file is licensed to you under the Apache License,
// Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
// or the MIT license (http://opensource.org/licenses/MIT),
// at your option.

// Unless required by applicable law or agreed to in writing,
// this software is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR REPRESENTATIONS OF ANY KIND, either express or
// implied. See the LICENSE-MIT and LICENSE-APACHE files for the
// specific language governing permissions and limitations under
// each license.#include "c2pa.h"

#ifndef C2PA_FSTREAM_H
#define C2PA_FSTREAM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../include/c2pa.h"

intptr_t reader(StreamContext *context, uint8_t *data, intptr_t len)
{
    // printf("reader: context = %0lx, data = %p, len = %zu\n", context, data, len);
    size_t count = fread(data, 1, len, (FILE *)context);
    // printf(" reader File %zu len = %zu, count = %zu\n", context, len, count);
    if (count != (size_t)len)
    {
        // do not report EOF as an error
        if (ferror((FILE *)context))
        {
            // printf("reader: result = %d, %s\n", -errno, strerror(errno));
            // errors returned via errno
            return -1;
        }
    }
    return count;
}

intptr_t seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence)
{

    // printf("seeker: context = %0lx, offset = %ld, whence = %d\n", context, offset, whence);
    long int result = fseek((FILE *)context, offset, whence);
    if (result != 0)
    {
        printf("seeker: result = %d, %s\n", -errno, strerror(errno));
        // errors returned via errno
        return -1;
    }
    // printf("seeker offset= %ld pos = %ld whence = %d\n", offset, ftell((FILE*)context), whence);
    return ftell((FILE *)context);
}

intptr_t writer(StreamContext *context, const uint8_t *data, intptr_t len)
{
    // printf("writer: context = %zu, data = %p, len = %zu\n", context, data, len);
    size_t count = fwrite(data, 1, len, (FILE *)context);
    if (count != (size_t) len)
    {
        if (ferror((FILE *)context))
        {
            // printf("writer: result = %d, %s\n", -errno, strerror(errno));
            // errors returned via errno
            return -1;
        }
    }
    return count;
}

intptr_t flusher(StreamContext *context)
{
    // printf("flusher: context = %zu\n", context);
    int result = fflush((FILE *)context);
    if (result != 0)
    {
        // errors returned via errno
        return -1;
    }
    return 0;
}

C2paStream *create_file_stream(FILE *file)
{
    if (file != NULL)
    {
        return c2pa_create_stream((StreamContext *)file, reader, seeker, writer, flusher);
    }
    return NULL;
}

void release_stream(C2paStream *stream)
{
    c2pa_release_stream(stream);
}

C2paStream *open_file_stream(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);
    if (file != NULL)
    {
        // printf("file open = %0lx\n", (unsigned long)file);
        return create_file_stream(file);
    }
    return NULL;
}

int close_file_stream(C2paStream *stream)
{
    FILE *file = (FILE *)stream->context;
    int result = fclose(file);
    c2pa_release_stream(stream);
    return result;
}

#endif