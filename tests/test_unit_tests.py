# Copyright 2023 Adobe. All rights reserved.
# This file is licensed to you under the Apache License,
# Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
# or the MIT license (http://opensource.org/licenses/MIT),
# at your option.

# Unless required by applicable law or agreed to in writing,
# this software is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR REPRESENTATIONS OF ANY KIND, either express or
# implied. See the LICENSE-MIT and LICENSE-APACHE files for the
# specific language governing permissions and limitations under
# each license.import unittest

import os
import io
import json
import unittest
from unittest.mock import mock_open, patch

from c2pa import  Builder, C2paError as Error,  Reader, C2paSigningAlg as SigningAlg, C2paSignerInfo, Signer,  sdk_version #,  load_settings_file

PROJECT_PATH = os.getcwd()

testPath = os.path.join(PROJECT_PATH, "tests", "fixtures", "C.jpg")

class TestC2paSdk(unittest.TestCase):
    def test_version(self):
        print(sdk_version())
        self.assertIn("0.8.0", sdk_version())


class TestReader(unittest.TestCase):
    def test_stream_read(self):
        with open(testPath, "rb") as file:
            with Reader("image/jpeg", file) as reader:
                json = reader.json()
                self.assertIn("C.jpg", json)

    def test_stream_read_and_parse(self):
        with open(testPath, "rb") as file:
            with Reader("image/jpeg", file) as reader:
                manifest_store = json.loads(reader.json())
                title = manifest_store["manifests"][manifest_store["active_manifest"]]["title"]
                self.assertEqual(title, "C.jpg")

    def test_json_decode_err(self):
        with self.assertRaises(Error.Io):
            with Reader("image/jpeg", "foo") as reader:
                manifest_store = reader.json()

    def test_reader_bad_format(self):
        with self.assertRaises(Error.NotSupported):
            with open(testPath, "rb") as file:
                with Reader("badFormat", file) as reader:
                    pass

    def test_settings_trust(self):
        #load_settings_file("tests/fixtures/settings.toml")
        with open(testPath, "rb") as file:
            with Reader("image/jpeg", file) as reader:
                json = reader.json()
                self.assertIn("C.jpg", json)

class TestBuilder(unittest.TestCase):
    # Define a manifest as a dictionary
    manifestDefinition = {
        "claim_generator": "python_test",
        "claim_generator_info": [{
            "name": "python_test",
            "version": "0.0.1",
        }],
        "format": "image/jpeg",
        "title": "Python Test Image",
        "ingredients": [],
        "assertions": [
            {   'label': 'stds.schema-org.CreativeWork',
                'data': {
                    '@context': 'http://schema.org/',
                    '@type': 'CreativeWork',
                    'author': [
                        {   '@type': 'Person',
                            'name': 'Gavin Peacock'
                        }
                    ]
                },
                'kind': 'Json'
            }
        ]
    }

    # Define a function that signs data with PS256 using a private key
    #def sign(data: bytes) -> bytes:
    #    key = open("tests/fixtures/ps256.pem","rb").read()
    #    return sign_ps256(data, key)

    # load the public keys from a pem file
    data_dir = "tests/fixtures/"
    certs = open(data_dir + "es256_certs.pem", "rb").read()
    key = open(data_dir + "es256_private.key", "rb").read()


    # Create a local Ps256 signer with certs and a timestamp server
    signer_info = C2paSignerInfo(
        alg=b"es256",
        sign_cert=certs,
        private_key=key,
        ta_url=None
    )

    signer = Signer.from_info(signer_info)
    #signer = create_signer(sign, SigningAlg.PS256, certs, "http://timestamp.digicert.com")

    def _read_manifest(self, reader):
        json_data = reader.json()
        self.assertIn("Python Test", json_data)
        self.assertNotIn("validation_status", json_data)

    def test_streams_sign(self):
        with open(testPath, "rb") as file, \
             io.BytesIO(bytearray()) as output, \
             Builder(TestBuilder.manifestDefinition) as builder:
            builder.sign(TestBuilder.signer, "image/jpeg", file, output)
            output.seek(0)
            with Reader("image/jpeg", output) as reader:
                self._read_manifest(reader)

    def test_archive_sign(self):
        with open(testPath, "rb") as file, \
             io.BytesIO(bytearray()) as archive:
            with Builder(TestBuilder.manifestDefinition) as builder:
                builder.to_archive(archive)
                archive.seek(0)
                with Builder.from_archive(archive) as builder2, \
                     io.BytesIO(bytearray()) as output:
                    builder2.sign(TestBuilder.signer, "image/jpeg", file, output)
                    output.seek(0)
                    with Reader("image/jpeg", output) as reader:
                        self._read_manifest(reader)

    def test_remote_sign(self):
        with open(testPath, "rb") as file, \
             io.BytesIO(bytearray()) as output, \
             Builder(TestBuilder.manifestDefinition) as builder:
            builder.set_no_embed()
            manifest_data = builder.sign(TestBuilder.signer, "image/jpeg", file, output)
            output.seek(0)
            with Reader("image/jpeg", output, manifest_data) as reader:
                self._read_manifest(reader)

if __name__ == '__main__':
    unittest.main()
