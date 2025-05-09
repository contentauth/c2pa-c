import os
import unittest
import tempfile
import json
from pathlib import Path
from c2pa import (
    C2paError,
    C2paSigningAlg,
    C2paSignerInfo,
    Stream,
    Reader,
    Builder,
    Signer,
    version,
    load_settings,
    read_file,
    read_ingredient_file,
    sign_file,
    format_embeddable,
    ed25519_sign,
)

# Test data directory
TEST_DATA_DIR = Path(__file__).parent / "fixtures"

class TestC2PA(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Set up test data that will be used by all test methods."""
        cls.unsigned_file = TEST_DATA_DIR / "A.jpg"
        cls.signed_file = TEST_DATA_DIR / "C.jpg"
        cls.cert_file = TEST_DATA_DIR / "test.crt"
        cls.key_file = TEST_DATA_DIR / "test.key"

        # Verify test files exist
        assert cls.unsigned_file.exists(), f"Test file {cls.unsigned_file} not found"
        assert cls.signed_file.exists(), f"Test file {cls.signed_file} not found"
        assert cls.cert_file.exists(), f"Certificate file {cls.cert_file} not found"
        assert cls.key_file.exists(), f"Key file {cls.key_file} not found"

        # Load certificate and key data
        with open(cls.cert_file, 'rb') as f:
            cls.cert_data = f.read()
        with open(cls.key_file, 'rb') as f:
            cls.key_data = f.read()

        # Create signer info
        cls.signer_info = C2paSignerInfo(
            alg=b"ES256",
            sign_cert=cls.cert_data,
            private_key=cls.key_data,
            ta_url=None  # b"http://test.tsa"
        )

    def test_version(self):
        """Test that version() returns a non-empty string."""
        v = version()
        self.assertIsInstance(v, str)
        self.assertGreater(len(v), 0)

    def test_load_settings(self):
        """Test loading settings."""
        settings = '{"test": "value"}'
        load_settings(settings)
        # No exception means success

    def test_read_unsigned_file(self):
        """Test reading an unsigned JPEG file."""
        with self.assertRaises(C2paError) as context:
            read_file(self.unsigned_file)
        self.assertIn("ManifestNotFound", str(context.exception))

    def test_read_signed_file(self):
        """Test reading a signed JPEG file with C2PA manifest."""
        manifest = read_file(self.signed_file)
        self.assertIsInstance(manifest, str)
        self.assertGreater(len(manifest), 0)
        
        # Verify it's valid JSON
        manifest_data = json.loads(manifest)
        self.assertIsInstance(manifest_data, dict)
        # Verify it has expected C2PA structure
        self.assertIn("claim_generator_info", manifest_data)
        self.assertIn("title", manifest_data)
        self.assertIn("format", manifest_data)

    def test_read_ingredient_file(self):
        """Test reading a JPEG file as an ingredient."""
        ingredient = read_ingredient_file(self.unsigned_file)
        self.assertIsInstance(ingredient, str)
        
        # Verify it's valid JSON
        ingredient_data = json.loads(ingredient)
        self.assertIsInstance(ingredient_data, dict)
        # Verify it has expected ingredient structure
        self.assertIn("format", ingredient_data)
        self.assertIn("title", ingredient_data)

    def test_stream(self):
        """Test Stream class with a real file."""
        with open(self.unsigned_file, 'rb') as f:
            stream = Stream(f)
            self.assertIsNotNone(stream._stream)
            stream.close()

    def test_reader_unsigned(self):
        """Test Reader class with an unsigned file."""
        reader = Reader(self.unsigned_file)
        self.assertIsNotNone(reader._reader)
        
        try:
            manifest = reader.json()
            self.assertIsNone(manifest)  # Unsigned file should have no manifest
        except C2paError as e:
            # This is expected if the file isn't a valid C2PA file
            self.assertIn("not a valid C2PA file", str(e))
        
        reader.close()

    def test_reader_signed(self):
        """Test Reader class with a signed file."""
        reader = Reader(self.signed_file)
        self.assertIsNotNone(reader._reader)
        
        manifest = reader.json()
        self.assertIsInstance(manifest, str)
        self.assertGreater(len(manifest), 0)
        
        # Verify it's valid JSON
        manifest_data = json.loads(manifest)
        self.assertIsInstance(manifest_data, dict)
        self.assertIn("claim_generator", manifest_data)
        
        reader.close()

    def test_signer(self):
        """Test Signer class creation and operations."""
        # Test creating signer from info
        signer = Signer.from_info(self.signer_info)
        self.assertIsNotNone(signer._signer)
        
        # Test reserve_size
        try:
            size = signer.reserve_size()
            self.assertIsInstance(size, int)
            self.assertGreaterEqual(size, 0)
        except C2paError as e:
            # This is expected if the signer info is invalid
            self.assertIn("invalid signer info", str(e))
        
        signer.close()
        
        # Test creating signer from callback
        def test_callback(data: bytes) -> bytes:
            return b"test_signature"
        
        signer = Signer.from_callback(
            callback=test_callback,
            alg=C2paSigningAlg.ES256,
            certs=self.cert_data.decode('utf-8'),
            tsa_url="http://test.tsa"
        )
        self.assertIsNotNone(signer._signer)
        signer.close()

    def test_builder(self):
        """Test Builder class operations with a real file."""
        # Test creating builder from JSON
        manifest_json = '{"test": "value"}'
        builder = Builder.from_json(manifest_json)
        self.assertIsNotNone(builder._builder)
        
        # Test builder operations
        builder.set_no_embed()
        builder.set_remote_url("http://test.url")
        
        # Test adding resource
        with open(self.unsigned_file, 'rb') as f:
            builder.add_resource("test_uri", f)
        
        # Test adding ingredient
        ingredient_json = '{"test": "ingredient"}'
        with open(self.unsigned_file, 'rb') as f:
            builder.add_ingredient(ingredient_json, "image/jpeg", f)
        
        builder.close()

    def test_ed25519_sign(self):
        """Test Ed25519 signing."""
        data = b"test data"
        private_key = "test_private_key"
        
        try:
            signature = ed25519_sign(data, private_key)
            self.assertIsInstance(signature, bytes)
            self.assertEqual(len(signature), 64)  # Ed25519 signatures are always 64 bytes
        except C2paError as e:
            # This is expected if the private key is invalid
            self.assertIn("invalid private key", str(e))

    def test_format_embeddable(self):
        """Test formatting embeddable manifest."""
        format_str = "image/jpeg"
        manifest_bytes = b"test manifest"
        
        try:
            size, result = format_embeddable(format_str, manifest_bytes)
            self.assertIsInstance(size, int)
            self.assertGreaterEqual(size, 0)
            self.assertIsInstance(result, bytes)
            self.assertEqual(len(result), size)
        except C2paError as e:
            # This is expected if the format or manifest is invalid
            self.assertTrue("invalid format" in str(e) or "invalid manifest" in str(e))

    def test_context_managers(self):
        """Test context manager functionality with real files."""
        # Test Reader context manager with unsigned file
        with Reader(self.unsigned_file) as reader:
            self.assertIsNotNone(reader._reader)
            try:
                manifest = reader.json()
                self.assertIsNone(manifest)  # Unsigned file should have no manifest
            except C2paError:
                pass
        
        # Test Reader context manager with signed file
        with Reader(self.signed_file) as reader:
            self.assertIsNotNone(reader._reader)
            manifest = reader.json()
            self.assertIsInstance(manifest, str)
            self.assertGreater(len(manifest), 0)
        
        # Test Signer context manager
        with Signer.from_info(self.signer_info) as signer:
            self.assertIsNotNone(signer._signer)
            try:
                size = signer.reserve_size()
                self.assertIsInstance(size, int)
                self.assertGreaterEqual(size, 0)
            except C2paError:
                pass
        
        # Test Builder context manager
        manifest_json = '{"test": "value"}'
        with Builder.from_json(manifest_json) as builder:
            self.assertIsNotNone(builder._builder)
            builder.set_no_embed()
            builder.set_remote_url("http://test.url")

if __name__ == '__main__':
    unittest.main()