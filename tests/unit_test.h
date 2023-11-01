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

#include "../include/c2pa.h"

//#define SIGN_INFO "{\"alg\" : \"es256\", \"tsa_url\" : \"http://timestamp.digicert.com\", \"signcert\": \"%s\", \"pkey\": \"%s\"}"

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
    } else {
        fprintf(stderr, "FAILED: unable to open file %s\n", filename);
        exit(1);
    }
    return buffer;
}

// these functions implement a poor person's test framework
void passed(const char* what, char *c2pa_str) {
    printf("PASSED: %s\n", what);
    c2pa_release_string(c2pa_str);
}

// assert that c2pa_str contains substr or exit
void assert_contains(const char* what, char *c2pa_str, const char *substr)
{
    if (strstr(c2pa_str, substr) == NULL)
    {
        fprintf(stderr, "FAILED %s: %s not found in %s\n", what, c2pa_str, substr);
        c2pa_release_string(c2pa_str);
        exit(1);
    }
    printf("PASSED: %s\n", what);
    c2pa_release_string(c2pa_str);
}



// assert that c2pa_str is not NULL or exit
void assert_not_null(const char* what, char *c2pa_str)
{
    if (c2pa_str == NULL)
    {
        char *err = c2pa_error();
        fprintf(stderr, "FAILED %s: %s\n", what, err);
        c2pa_release_string(err);
        c2pa_release_string(c2pa_str);
        exit(1);
    } 
    printf("PASSED: %s\n", what);
    c2pa_release_string(c2pa_str);
}


// assert if c2pa_str is NULL and we have the expected error
void assert_null(const char* what, char *c2pa_str, const char *err_str)
{
    if (c2pa_str == NULL)
    {
        char *err = c2pa_error();
        if (strstr(err, err_str) == NULL)
        {
            fprintf(stderr, "FAILED %s: \"%s\" not found in \"%s\"\n", what, err_str, err);
            c2pa_release_string(err);
            exit(1);
        }
        printf("PASSED: %s: \n", what);
    } else {
        fprintf(stderr, "FAILED %s: expected NULL\n", what);
        c2pa_release_string(c2pa_str);
        exit(1);
    }
}
