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
#include "../include/file_stream.h"
#include "unit_test.h"

int main(void)
{
    char *version = c2pa_version();
    assert_contains("version", version, "c2pa-c/0.");

    char *result1 = c2pa_read_file("tests/fixtures/C.jpg", NULL);
    assert_str_not_null("c2pa_read_file_no_data_dir", result1);

    char *result = c2pa_read_file("tests/fixtures/C.jpg", "target/tmp");
    assert_str_not_null("c2pa_read_file", result);

    result = c2pa_read_ingredient_file("tests/fixtures/C.jpg", "target/ingredient");
    assert_str_not_null("c2pa_ingredient_from_file", result);

    CStream* input_stream = open_file_stream("tests/fixtures/C.jpg", "rb");
    assert_not_null("open_file_stream", input_stream);

    ManifestStore* manifest_store = c2pa_manifest_store_read("image/jpeg", input_stream);
    assert_not_null("manifest_store_from_stream", manifest_store);

    close_file_stream(input_stream);

    char* json = c2pa_manifest_store_json(manifest_store);
    assert_not_null("c2pa_manifest_store_json", json);
    // printf("manifest json = %s\n", json);

    // we should fetch the active manifest and retrieve the identifier from the thumbnail in that manifest 
    char *uri = findValueByKey(json, "identifier");
    if (uri == NULL) {
        fprintf(stderr, "FAILED: unable to find identifier in manifest json\n");
        exit(1);
    }

	//Open a file to write the thumbnail into
    CStream* thumb_stream = open_file_stream("target/thumb_c.jpg", "wb");
    assert_not_null("open_file_stream thumbnail", thumb_stream);

    // write the thumbnail resource to the stream
    int res = c2pa_manifest_store_get_resource(manifest_store, uri, thumb_stream);
    free(uri);
    assert_int("c2pa_manifest_reader_get_resource", res);

    c2pa_release_manifest_store(manifest_store);
 
    char *certs = load_file("tests/fixtures/es256_certs.pem");
    char *private_key = load_file("tests/fixtures/es256_private.key");

    char *manifest = load_file("tests/fixtures/training.json");

    // create a sign_info struct
    C2paSignerInfo sign_info = {.alg = "es256", .sign_cert = certs, .private_key = private_key, .ta_url = "http://timestamp.digicert.com"};

    result = c2pa_sign_file("tests/fixtures/C.jpg", "target/tmp/earth.jpg", manifest, &sign_info, "tests/fixtures");
    assert_not_null("c2pa_sign_file_ok", result);

    result = c2pa_sign_file("tests/fixtures/foo.jpg", "target/tmp/earth.jpg", manifest, &sign_info, "tests/fixtures");
    assert_null("c2pa_sign_file_not_found", result, "FileNotFound");

    result = c2pa_sign_file("tests/fixtures/es256_certs.pem", "target/tmp/earth.jpg", manifest, &sign_info, "tests/fixtures");
    assert_null("c2pa_sign_file_not_supported", result, "NotSupported");

    free(certs);
    free(private_key);
}