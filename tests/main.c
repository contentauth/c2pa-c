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

const char* manifest = "{\
    \"claim_generator\":\"test\",\
    \"ingredients\":[], \
    \"assertions\":[] \
}";

#include <stdio.h>
#include <stdlib.h>

#define SIGN_INFO "{\"alg\" : \"es256\", \"tsa_url\" : \"http://timestamp.digicert.com\", \"signcert\": \"%s\", \"pkey\": \"%s\"}"

char* load_file(const char* filename) {
    char* buffer = NULL;
    long file_size;
    FILE* fp = fopen(filename, "rb");  // Open file in binary mode

    if (fp != NULL) {
        // Determine file size
        fseek(fp, 0L, SEEK_END);
        file_size = ftell(fp);
        rewind(fp);

        // Allocate buffer
        buffer = (char*) malloc(file_size + 1);
        if (buffer != NULL) {
            // Read file into buffer
            fread(buffer, 1, file_size, fp);
            buffer[file_size] = '\0';  // Add null terminator
        }
        fclose(fp);
    }
    return buffer;
}

int main(void) {

    char *version = c2pa_version();
    printf("version = %s\n", version);
    c2pa_release_string(version);

    char *formats = c2pa_supported_formats();
    printf("supported formats = %s\n", formats);
    c2pa_release_string(formats);

    char* result = c2pa_read_file("tests/fixtures/C.jpg", "target/tmp");
    if (result != NULL) {
        printf("read = %s\n", result);
        c2pa_release_string(result);
    } else {
        char * err = c2pa_error();
        printf("read error = %s\n", err);
        c2pa_release_string(err);
    }

    result = c2pa_ingredient_from_file("tests/fixtures/C.jpg", "target/ingredient");
    if (result != NULL) {
        printf("ingredient = %s\n", result);
        c2pa_release_string(result);
    } else {
        char * err = c2pa_error();
        printf("ingredient error = %s\n", err);
        c2pa_release_string(err);
    }

    // // load es256_certs.pem file into a char array
    char * certs = load_file("tests/fixtures/es256_certs.pem");
    // // load es256_private_key.pem file into a char array
    char * private_key = load_file("tests/fixtures/es256_private.key");

    if (certs && private_key) {
        // create a sign_info struct
        C2paSignerInfo sign_info = { .alg = "es256", .tsa_url = "http://timestamp.digicert.com", .signcert = certs, .pkey = private_key };

        result = c2pa_add_manifest_to_file("tests/fixtures/C.jpg", "target/tmp/earth.jpg", manifest, sign_info, "tests/fixtures");
        if (result != NULL) {
            printf("added manifest to %s\n", "target/tmp/earth.jpg");
            c2pa_release_string(result);
        } else {
            char * err = c2pa_error();
            printf("add manifest error = %s\n", err);
            c2pa_release_string(err);
        }
        free(certs);
        free(private_key);
    } else {
        printf("unable to load certs or private key\n");
    }
}