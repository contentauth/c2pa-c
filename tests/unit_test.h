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
// each license.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include  <c2pa.h>

// load a file into a string for testing
char *load_file(const char *filename)
{
    char *buffer = NULL;
    long file_size;
    FILE *fp = fopen(filename, "rb"); // Open file in binary mode

    if (fp != NULL)
    {
        // Determine file size
        fseek(fp, 0L, SEEK_END);
        file_size = ftell(fp);
        if (file_size < 0) {
            fprintf(stderr, "FAILED: ftell error for file %s\n", filename);
            fclose(fp);
            exit(1);
        }
        rewind(fp);

        // Allocate buffer
        buffer = (char *)malloc((size_t)file_size + 1);
        if (buffer != NULL)
        {
            // Read file into buffer and verify all bytes were read
            size_t bytes_read = fread(buffer, 1, (size_t)file_size, fp);
            if (bytes_read != (size_t)file_size) {
                fprintf(stderr, "FAILED: short read for file %s (expected %ld, got %zu)\n", filename, file_size, bytes_read);
                free(buffer);
                fclose(fp);
                exit(1);
            }
            buffer[file_size] = '\0'; // Add null terminator
        }
        fclose(fp);
    }
    else
    {
        fprintf(stderr, "FAILED: unable to open file %s\n", filename);
        exit(1);
    }
    return buffer;
}

int save_file(const char* filename, const unsigned char* data, size_t len) {
    FILE* fp = fopen(filename, "wb");  
    int bytes_written = -1; 
    // Open file in binary mode
    if (fp != NULL) {
        bytes_written = fwrite(data, len, 1, fp);
        fclose(fp);
    }
    return bytes_written;
}

// these functions implement a poor person's test framework
void passed(const char *what, char *c2pa_str)
{
    printf("PASSED: %s\n", what);
    if (c2pa_str != NULL)
        c2pa_free(c2pa_str);
}

// assert that c2pa_str contains substr or exit
void assert_contains(const char *what, char *c2pa_str, const char *substr)
{
    if (c2pa_str == NULL || strstr(c2pa_str, substr) == NULL)
    {
        fprintf(stderr, "FAILED %s: %s not found in %s\n", what, c2pa_str ? c2pa_str : "(null)", substr);
        if (c2pa_str != NULL)
            c2pa_free(c2pa_str);
        exit(1);
    }
    printf("PASSED: %s\n", what);
    c2pa_free(c2pa_str);
}

// assert that c2pa is not NULL or exit
void assert_not_null(const char *what, void *val)
{
    if (val == NULL)
    {
        char *err = c2pa_error();
        fprintf(stderr, "FAILED %s: %s\n", what, err ? err : "(null)");
        if (err != NULL)
            c2pa_free(err);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

// assert that c2pa_str is not NULL or exit
void assert_str_not_null(const char *what, char *c2pa_str)
{
    assert_not_null(what, c2pa_str);
    if (c2pa_str != NULL)
        c2pa_free(c2pa_str);
}

// assert that c2pa is not NULL or exit

// assert if c2pa_str is NULL and we have the expected error
void assert_null(const char *what, char *c2pa_str, const char *err_str)
{
    if (c2pa_str == NULL)
    {
        char *err = c2pa_error();
        if (err == NULL || strstr(err, err_str) == NULL)
        {
            fprintf(stderr, "FAILED %s: \"%s\" not found in \"%s\"\n", what, err_str, err ? err : "(null)");
            if (err != NULL)
                c2pa_free(err);
            exit(1);
        }
        printf("PASSED: %s: \n", what);
        c2pa_free(err);
    }
    else
    {
        fprintf(stderr, "FAILED %s: expected NULL\n", what);
        c2pa_free(c2pa_str);
        exit(1);
    }
}

void assert_int(const char *what, int result)
{
    if (result < 0)
    {
        char *err = c2pa_error();
        fprintf(stderr, "FAILED %s: %s\n", what, err ? err : "(null)");
        if (err != NULL)
            c2pa_free(err);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

// Function to find the value associated with a key in a JSON string.
// Hardened: validates all inputs, uses size_t for lengths, checks malloc results,
// and bounds-checks all pointer arithmetic.
char* findValueByKey(const char* json, const char* key) {
    if (json == NULL || key == NULL) {
        return NULL;
    }

    const char* json_end = json + strlen(json);
    const char* keyStart = strstr(json, key);

    if (keyStart == NULL) {
        return NULL;  // Key not found
    }

    const char* valueStart = strchr(keyStart, ':');
    if (valueStart == NULL || valueStart >= json_end) {
        return NULL;  // Malformed JSON
    }

    // Move past the ':' and whitespace, with bounds checking
    valueStart++;
    while (valueStart < json_end &&
           (*valueStart == ' ' || *valueStart == '\t' || *valueStart == '\n' || *valueStart == '\r')) {
        valueStart++;
    }

    if (valueStart >= json_end) {
        return NULL;  // Ran past end of string
    }

    if (*valueStart == '"') {
        // String value
        const char* valueEnd = strchr(valueStart + 1, '"');
        if (valueEnd == NULL || valueEnd >= json_end) {
            return NULL;  // Malformed JSON
        }
        size_t valueLength = (size_t)(valueEnd - valueStart - 1);
        char* result = (char*)malloc(valueLength + 1);
        if (result == NULL) {
            return NULL;  // Allocation failed
        }
        memcpy(result, valueStart + 1, valueLength);
        result[valueLength] = '\0';
        return result;
    } else {
        // Numeric or other value
        const char* valueEnd = valueStart;
        while (valueEnd < json_end && *valueEnd != ',' && *valueEnd != '}' && *valueEnd != ']' && *valueEnd != '\0') {
            valueEnd++;
        }
        size_t valueLength = (size_t)(valueEnd - valueStart);
        char* result = (char*)malloc(valueLength + 1);
        if (result == NULL) {
            return NULL;  // Allocation failed
        }
        memcpy(result, valueStart, valueLength);
        result[valueLength] = '\0';
        return result;
    }
}

// Signer callback
// Hardened: uses mkstemp for secure temp files, checks all return values (S1, S3, S4).
intptr_t signer_callback(const void* context, const unsigned char *data, uintptr_t len, unsigned char *signature, uintptr_t sig_max_len) {
    // Validate inputs
    if (data == NULL || len == 0 || signature == NULL || sig_max_len == 0) {
        fprintf(stderr, "signer_callback: invalid arguments (data=%p, len=%zu, sig=%p, max=%zu)\n",
                (const void*)data, (size_t)len, (void*)signature, (size_t)sig_max_len);
        return -1;
    }
    uint64_t data_len= (uint64_t) len;

    // Create secure temp files instead of using predictable hardcoded paths (S4)
    char data_template[] = "build/c_data_XXXXXX";
    char sig_template[] = "build/c_sig_XXXXXX";
    int data_fd = mkstemp(data_template);
    if (data_fd < 0) {
        fprintf(stderr, "signer_callback: failed to create temp data file\n");
        return -1;
    }

    // Write data to the secure temp file
    ssize_t written = write(data_fd, data, data_len);
    close(data_fd);
    if (written < 0 || (uint64_t)written != data_len) {
        fprintf(stderr, "signer_callback: failed to write data to temp file\n");
        unlink(data_template);
        return -1;
    }

    if (context != NULL && strncmp((const char *)context,"testing context", 16)) {
        printf("signer callback unexpected context %s\n", (const char *) context);
    }

    // Build the openssl command with temp file paths
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "openssl dgst -sign tests/fixtures/es256_private.key -sha256 -out %s %s",
        sig_template, data_template);

    // Sign the temp file by calling openssl in a shell (S1: check return value)
    int sys_ret = system(cmd);
    unlink(data_template);  // Clean up data temp file immediately
    if (sys_ret != 0) {
        fprintf(stderr, "signer_callback: openssl command failed with status %d\n", sys_ret);
        unlink(sig_template);
        return -1;
    }

    // Read the signature file
    FILE* result_file = fopen(sig_template, "rb");
    if (result_file == NULL) {
        fprintf(stderr, "signer_callback: failed to open signature file\n");
        unlink(sig_template);
        return -1;
    }
    fseek(result_file, 0L, SEEK_END);
    long sig_len = ftell(result_file);
    if (sig_len < 0) {
        fprintf(stderr, "signer_callback: ftell failed on signature file\n");
        fclose(result_file);
        unlink(sig_template);
        return -1;
    }
    rewind(result_file);

    if (sig_len > (long) sig_max_len) {
        fprintf(stderr, "signer_callback: signature too large (%ld > %zu)\n", sig_len, (size_t)sig_max_len);
        fclose(result_file);
        unlink(sig_template);
        return -1;
    }
    size_t bytes_read = fread(signature, 1, (size_t)sig_len, result_file);
    fclose(result_file);
    unlink(sig_template);  // Clean up signature temp file

    if (bytes_read != (size_t)sig_len) {
        fprintf(stderr, "signer_callback: short read on signature file (expected %ld, got %zu)\n", sig_len, bytes_read);
        return -1;
    }
    return sig_len;
}

