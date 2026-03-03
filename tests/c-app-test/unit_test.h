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
        rewind(fp);

        // Allocate buffer
        buffer = (char *)malloc(file_size + 1);
        if (buffer != NULL)
        {
            // Read file into buffer
            fread(buffer, 1, file_size, fp);
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

// Function to find the value associated with a key in a JSON string
char* findValueByKey(const char* json, const char* key) {
    const char* keyStart = strstr(json, key);

    if (keyStart == NULL) {
        return NULL;  // Key not found
    }

    const char* valueStart = strchr(keyStart, ':');
    if (valueStart == NULL) {
        return NULL;  // Malformed JSON
    }

    // Move past the ':' and whitespace
    valueStart++;
    while (*valueStart == ' ' || *valueStart == '\t' || *valueStart == '\n' || *valueStart == '\r') {
        valueStart++;
    }

    if (*valueStart == '"') {
        // String value
        const char* valueEnd = strchr(valueStart + 1, '"');
        if (valueEnd == NULL) {
            return NULL;  // Malformed JSON
        }
        int valueLength = valueEnd - valueStart - 1;
        char* result = (char*)malloc(valueLength + 1);
        strncpy(result, valueStart + 1, valueLength);
        result[valueLength] = '\0';
        return result;
    } else {
        // Numeric or other value
        const char* valueEnd = valueStart;
        while (*valueEnd != ',' && *valueEnd != '}' && *valueEnd != ']' && *valueEnd != '\0') {
            valueEnd++;
        }
        int valueLength = valueEnd - valueStart;
        char* result = (char*)malloc(valueLength + 1);
        strncpy(result, valueStart, valueLength);
        result[valueLength] = '\0';
        return result;
    }
}

// Signer callback
intptr_t signer_callback(const void* context, const unsigned char *data, uintptr_t len, unsigned char *signature, uintptr_t sig_max_len) {
    uint64_t data_len= (uint64_t) len;
    // printf("signer callback: data = %p, len = %ld\n", data, data_len);
    // write data to be signed to a temp file
    int result = save_file("build/c_data.bin", data, data_len);
    if (result < 0) {
        printf("signing failed");
        return -1;
    }
    if (context != NULL && strncmp((const char *)context,"testing context", 16)) {
        printf("signer callback unexpected context %s\n", (const char *) context);
    }
    // sign the temp file by calling openssl in a shell
    system("openssl dgst -sign tests/fixtures/es256_private.key -sha256 -out build/c_signature.sig build/c_data.bin");

    // read the signature file
    FILE* result_file = fopen("build/c_signature.sig", "rb");
    if (result_file == NULL) {
        printf("signing failed");
        return -1;
    }
    fseek(result_file, 0L, SEEK_END);
    long sig_len = ftell(result_file);
    rewind(result_file);

    if (sig_len > (long) sig_max_len) {
        printf("signing failed");
        return -1;
    }
    fread(signature, 1, sig_len, result_file);
    fclose(result_file);
    return sig_len;
}

