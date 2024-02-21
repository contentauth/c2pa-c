// Copyright 2023 Adobe. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "c2pa.h"

ssize_t reader(size_t context, uint8_t *data, size_t len) {
    // printf("reader: context = %0lx, data = %p, len = %zu\n", context, data, len);
    size_t count = fread(data, 1, len, (FILE*)context);
    if (count != len) {
        if (ferror((FILE*)context)) {
             printf("reader: result = %d, %s\n", -errno, strerror(errno));
            return -errno;
        }
    }
    return count;
}

long seeker(size_t context,long int offset, int whence) {

    // printf("seeker: context = %0lx, offset = %ld, whence = %d\n", context, offset, whence);
    long int result = fseek((FILE*)context, offset, whence);
    if (result != 0) {
        printf("seeker: result = %d, %s\n", -errno, strerror(errno));
        return -errno;
    }
    return ftell((FILE*)context);
}

ssize_t writer(size_t context, uint8_t *data, size_t len) {
    // printf("writer: context = %zu, data = %p, len = %zu\n", context, data, len);
    size_t count = fwrite(data, 1, len, (FILE*)context);
        if (count != len) {
        if (ferror((FILE*)context)) {
            printf("writer: result = %d, %s\n", -errno, strerror(errno));
            return -errno;
        }
    }
    return count;
}

ssize_t flusher(size_t context) {
    // printf("flusher: context = %zu\n", context);
    int result = fflush((FILE*)context);
    if (result != 0) {
        return -errno;
    }
    return 0;
}   

CStream* create_file_stream(FILE *file) {
    if (file != NULL) {
      return c2pa_create_stream((StreamContext*)file, (ReadCallback)reader, (SeekCallback) seeker, (WriteCallback)writer, (FlushCallback)flusher);
    }
    return NULL;
}

void release_stream(CStream* stream) {
    c2pa_release_stream(stream);
}

CStream* open_file_stream(const char *path, const char* mode) {
    FILE *file = fopen(path, mode);
    if (file != NULL) {
        // printf("file open = %0lx\n", (unsigned long)file);
        return create_file_stream(file);
    }
    return NULL;
}

int close_file_stream(CStream* stream) {
    FILE *file = (FILE*)stream->context;
    int result = fclose(file);
    c2pa_release_stream(stream);
    return result;
}
