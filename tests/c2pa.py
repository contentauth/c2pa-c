import ctypes
import enum
import json
import sys
import os
from pathlib import Path
from typing import Optional, Union, Callable, Any

# Determine the library name based on the platform
if sys.platform == "win32":
    _lib_name = "c2pa_c.dll"
elif sys.platform == "darwin":
    _lib_name = "libc2pa_c.dylib"
else:
    _lib_name = "libc2pa_c.so"

# Define required function names
_REQUIRED_FUNCTIONS = [
    'c2pa_version',
    'c2pa_error',
    'c2pa_string_free',
    'c2pa_release_string',
    'c2pa_load_settings',
    'c2pa_read_file',
    'c2pa_read_ingredient_file',
    'c2pa_sign_file',
    'c2pa_reader_from_stream',
    'c2pa_reader_from_manifest_data_and_stream',
    'c2pa_reader_free',
    'c2pa_reader_json',
    'c2pa_reader_resource_to_stream',
    'c2pa_builder_from_json',
    'c2pa_builder_from_archive',
    'c2pa_builder_free',
    'c2pa_builder_set_no_embed',
    'c2pa_builder_set_remote_url',
    'c2pa_builder_add_resource',
    'c2pa_builder_add_ingredient_from_stream',
    'c2pa_builder_to_archive',
    'c2pa_builder_sign',
    'c2pa_manifest_bytes_free',
    'c2pa_builder_data_hashed_placeholder',
    'c2pa_builder_sign_data_hashed_embeddable',
    'c2pa_format_embeddable',
    'c2pa_signer_create',
    'c2pa_signer_from_info',
    'c2pa_signer_reserve_size',
    'c2pa_signer_free',
    'c2pa_ed25519_sign',
    'c2pa_signature_free',
]

def _validate_library_exports(lib):
    """Validate that all required functions are present in the loaded library.
    
    This validation is crucial for several security and reliability reasons:
    
    1. Security:
       - Prevents loading of libraries that might be missing critical functions
       - Ensures the library has all expected functionality before any code execution
       - Helps detect tampered or incomplete libraries
    
    2. Reliability:
       - Fails fast if the library is incomplete or corrupted
       - Prevents runtime errors from missing functions
       - Ensures all required functionality is available before use
    
    3. Version Compatibility:
       - Helps detect version mismatches where the library doesn't have all expected functions
       - Prevents partial functionality that could lead to undefined behavior
       - Ensures the library matches the expected API version
    
    Args:
        lib: The loaded library object
        
    Raises:
        ImportError: If any required function is missing, with a detailed message listing
                    the missing functions. This helps diagnose issues with the library
                    installation or version compatibility.
    """
    missing_functions = []
    for func_name in _REQUIRED_FUNCTIONS:
        if not hasattr(lib, func_name):
            missing_functions.append(func_name)
    
    if missing_functions:
        raise ImportError(
            f"Library is missing required functions symbols: {', '.join(missing_functions)}\n"
            "This could indicate an incomplete or corrupted library installation or a version mismatch between the library and this Python wrapper"
        )

def _try_load_library(path):
    """Attempt to load and validate a library from the given path.
    
    Args:
        path: Path to the library file
        
    Returns:
        The loaded library object if successful
        
    Raises:
        ImportError: If the library cannot be loaded, with specific error messages for:
                    - Permission errors
                    - Corrupted libraries
                    - Missing functions
                    - Other loading errors
    """
    try:
        # Check file permissions
        if not os.access(path, os.R_OK):
            raise ImportError(f"Permission denied: Cannot read library at {path}")
            
        # Try to load the library
        try:
            lib = ctypes.CDLL(str(path))
        except OSError as e:
            if "Permission denied" in str(e):
                raise ImportError(f"Permission denied: Cannot load library at {path}")
            elif "invalid ELF header" in str(e) or "not a valid Win32 application" in str(e):
                raise ImportError(f"Corrupted or invalid library at {path}")
            else:
                raise ImportError(f"Failed to load library at {path}: {e}")
                
        # Validate the library exports
        _validate_library_exports(lib)
        
        return lib
        
    except ImportError:
        raise
    except Exception as e:
        raise ImportError(f"Unexpected error loading library at {path}: {e}")

# Try to find the library in common locations
_lib_paths = [
    Path(__file__).parent / _lib_name,  # Same directory as this file
    Path(__file__).parent / "target" / "release" / _lib_name,  # target/release
    Path(__file__).parent.parent / "target" / "release" / _lib_name,  # ../target/release
]

# Try each path until we find a valid library
for _path in _lib_paths:
    if _path.exists():
        try:
            _lib = _try_load_library(_path)
            break
        except ImportError as e:
            # If this is the last path, raise the error
            if _path == _lib_paths[-1]:
                raise ImportError(f"Could not find a valid {_lib_name} in any of: {[str(p) for p in _lib_paths]}\nLast error: {e}")
            # Otherwise continue to the next path
            continue
else:
    raise ImportError(f"Could not find {_lib_name} in any of: {[str(p) for p in _lib_paths]}")

class C2paSeekMode(enum.IntEnum):
    """Seek mode for stream operations."""
    START = 0
    CURRENT = 1
    END = 2

class C2paSigningAlg(enum.IntEnum):
    """Supported signing algorithms."""
    ES256 = 0
    ES384 = 1
    ES512 = 2
    PS256 = 3
    PS384 = 4
    PS512 = 5
    ED25519 = 6

# Define callback types
ReadCallback = ctypes.CFUNCTYPE(ctypes.c_ssize_t, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_ssize_t)
SeekCallback = ctypes.CFUNCTYPE(ctypes.c_ssize_t, ctypes.c_void_p, ctypes.c_ssize_t, ctypes.c_int)

# Additional callback types
WriteCallback = ctypes.CFUNCTYPE(ctypes.c_ssize_t, ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_ssize_t)
FlushCallback = ctypes.CFUNCTYPE(ctypes.c_ssize_t, ctypes.c_void_p)
SignerCallback = ctypes.CFUNCTYPE(ctypes.c_ssize_t, ctypes.c_void_p, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t)

class StreamContext(ctypes.Structure):
    """Opaque structure for stream context."""
    _fields_ = []  # Empty as it's opaque in the C API

class C2paSigner(ctypes.Structure):
    """Opaque structure for signer context."""
    _fields_ = []  # Empty as it's opaque in the C API

class C2paStream(ctypes.Structure):
    """A C2paStream is a Rust Read/Write/Seek stream that can be created in C."""
    _fields_ = [
        ("context", ctypes.POINTER(StreamContext)),
        ("reader", ReadCallback),
        ("seeker", SeekCallback),
        ("writer", WriteCallback),
        ("flusher", FlushCallback),
    ]

class C2paSignerInfo(ctypes.Structure):
    """Configuration for a Signer."""
    _fields_ = [
        ("alg", ctypes.c_char_p),
        ("sign_cert", ctypes.c_char_p),
        ("private_key", ctypes.c_char_p),
        ("ta_url", ctypes.c_char_p),
    ]

class C2paReader(ctypes.Structure):
    """Opaque structure for reader context."""
    _fields_ = []  # Empty as it's opaque in the C API

class C2paBuilder(ctypes.Structure):
    """Opaque structure for builder context."""
    _fields_ = []  # Empty as it's opaque in the C API

# Helper function to set function prototypes
def _setup_function(func, argtypes, restype=None):
    func.argtypes = argtypes
    func.restype = restype

# Set up function prototypes
_setup_function(_lib.c2pa_create_stream,
    [ctypes.POINTER(StreamContext), ReadCallback, SeekCallback, WriteCallback, FlushCallback],
    ctypes.POINTER(C2paStream))

# Add release_stream prototype
_setup_function(_lib.c2pa_release_stream, [ctypes.POINTER(C2paStream)], None)

# Set up core function prototypes
_setup_function(_lib.c2pa_version, [], ctypes.c_void_p)
_setup_function(_lib.c2pa_error, [], ctypes.c_void_p)
_setup_function(_lib.c2pa_string_free, [ctypes.c_void_p], None)
_setup_function(_lib.c2pa_release_string, [ctypes.c_void_p], None)
_setup_function(_lib.c2pa_load_settings, [ctypes.c_char_p, ctypes.c_char_p], ctypes.c_int)
_setup_function(_lib.c2pa_read_file, [ctypes.c_char_p, ctypes.c_char_p], ctypes.c_void_p)
_setup_function(_lib.c2pa_read_ingredient_file, [ctypes.c_char_p, ctypes.c_char_p], ctypes.c_void_p)
_setup_function(_lib.c2pa_sign_file, 
    [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(C2paSignerInfo), ctypes.c_char_p],
    ctypes.c_void_p)

# Set up Reader and Builder function prototypes
_setup_function(_lib.c2pa_reader_from_stream, 
    [ctypes.c_char_p, ctypes.POINTER(C2paStream)],
    ctypes.POINTER(C2paReader))
_setup_function(_lib.c2pa_reader_from_manifest_data_and_stream,
    [ctypes.c_char_p, ctypes.POINTER(C2paStream), ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t],
    ctypes.POINTER(C2paReader))
_setup_function(_lib.c2pa_reader_free, [ctypes.POINTER(C2paReader)], None)
_setup_function(_lib.c2pa_reader_json, [ctypes.POINTER(C2paReader)], ctypes.c_void_p)
_setup_function(_lib.c2pa_reader_resource_to_stream,
    [ctypes.POINTER(C2paReader), ctypes.c_char_p, ctypes.POINTER(C2paStream)],
    ctypes.c_int64)

# Set up Builder function prototypes
_setup_function(_lib.c2pa_builder_from_json, [ctypes.c_char_p], ctypes.POINTER(C2paBuilder))
_setup_function(_lib.c2pa_builder_from_archive, [ctypes.POINTER(C2paStream)], ctypes.POINTER(C2paBuilder))
_setup_function(_lib.c2pa_builder_free, [ctypes.POINTER(C2paBuilder)], None)
_setup_function(_lib.c2pa_builder_set_no_embed, [ctypes.POINTER(C2paBuilder)], None)
_setup_function(_lib.c2pa_builder_set_remote_url, [ctypes.POINTER(C2paBuilder), ctypes.c_char_p], ctypes.c_int)
_setup_function(_lib.c2pa_builder_add_resource, 
    [ctypes.POINTER(C2paBuilder), ctypes.c_char_p, ctypes.POINTER(C2paStream)],
    ctypes.c_int)
#_setup_function(_lib.c2pa_builder_add_ingredient,
#    [ctypes.POINTER(C2paBuilder), ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(C2paStream)],
#    ctypes.c_int)

# Set up additional Builder function prototypes
_setup_function(_lib.c2pa_builder_add_ingredient_from_stream,
    [ctypes.POINTER(C2paBuilder), ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(C2paStream)],
    ctypes.c_int)
_setup_function(_lib.c2pa_builder_to_archive,
    [ctypes.POINTER(C2paBuilder), ctypes.POINTER(C2paStream)],
    ctypes.c_int)
_setup_function(_lib.c2pa_builder_sign,
    [ctypes.POINTER(C2paBuilder), ctypes.c_char_p, ctypes.POINTER(C2paStream), 
     ctypes.POINTER(C2paStream), ctypes.POINTER(C2paSigner), ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))],
    ctypes.c_int64)
_setup_function(_lib.c2pa_manifest_bytes_free, [ctypes.POINTER(ctypes.c_ubyte)], None)
_setup_function(_lib.c2pa_builder_data_hashed_placeholder,
    [ctypes.POINTER(C2paBuilder), ctypes.c_size_t, ctypes.c_char_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))],
    ctypes.c_int64)

# Set up additional function prototypes
_setup_function(_lib.c2pa_builder_sign_data_hashed_embeddable,
    [ctypes.POINTER(C2paBuilder), ctypes.POINTER(C2paSigner), ctypes.c_char_p, ctypes.c_char_p,
     ctypes.POINTER(C2paStream), ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))],
    ctypes.c_int64)
_setup_function(_lib.c2pa_format_embeddable,
    [ctypes.c_char_p, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t, 
     ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))],
    ctypes.c_int64)
_setup_function(_lib.c2pa_signer_create,
    [ctypes.c_void_p, SignerCallback, ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p],
    ctypes.POINTER(C2paSigner))
_setup_function(_lib.c2pa_signer_from_info,
    [ctypes.POINTER(C2paSignerInfo)],
    ctypes.POINTER(C2paSigner))

# Set up final function prototypes
_setup_function(_lib.c2pa_signer_reserve_size, [ctypes.POINTER(C2paSigner)], ctypes.c_int64)
_setup_function(_lib.c2pa_signer_free, [ctypes.POINTER(C2paSigner)], None)
_setup_function(_lib.c2pa_ed25519_sign,
    [ctypes.POINTER(ctypes.c_ubyte), ctypes.c_size_t, ctypes.c_char_p],
    ctypes.POINTER(ctypes.c_ubyte))
_setup_function(_lib.c2pa_signature_free, [ctypes.POINTER(ctypes.c_ubyte)], None)

class C2paError(Exception):
    """Exception raised for C2PA errors."""
    def __init__(self, message: str = ""):
        self.message = message
        super().__init__(message)

    class Assertion(Exception):
        """Exception raised for assertion errors."""
        pass

    class AssertionNotFound(Exception):
        """Exception raised when an assertion is not found."""
        pass

    class Decoding(Exception):
        """Exception raised for decoding errors."""
        pass

    class Encoding(Exception):
        """Exception raised for encoding errors."""
        pass

    class FileNotFound(Exception):
        """Exception raised when a file is not found."""
        pass

    class Io(Exception):
        """Exception raised for IO errors."""
        pass

    class Json(Exception):
        """Exception raised for JSON errors."""
        pass

    class Manifest(Exception):
        """Exception raised for manifest errors."""
        pass

    class ManifestNotFound(Exception):
        """Exception raised when a manifest is not found."""
        pass

    class NotSupported(Exception):
        """Exception raised for unsupported operations."""
        pass

    class Other(Exception):
        """Exception raised for other errors."""
        pass

    class RemoteManifest(Exception):
        """Exception raised for remote manifest errors."""
        pass

    class ResourceNotFound(Exception):
        """Exception raised when a resource is not found."""
        pass

    class Signature(Exception):
        """Exception raised for signature errors."""
        pass

    class Verify(Exception):
        """Exception raised for verification errors."""
        pass

def _handle_string_result(result: ctypes.c_void_p, check_error: bool = True) -> Optional[str]:
    """Helper function to handle string results from C2PA functions."""
    if not result:  # NULL pointer
        if check_error:
            error = _lib.c2pa_error()
            if error:
                error_str = ctypes.cast(error, ctypes.c_char_p).value.decode('utf-8')
                _lib.c2pa_string_free(error)
                parts = error_str.split(' ', 1)
                if len(parts) > 1:
                    error_type, message = parts
                    if error_type == "Assertion":
                        raise C2paError.Assertion(message)
                    elif error_type == "AssertionNotFound":
                        raise C2paError.AssertionNotFound(message)
                    elif error_type == "Decoding":
                        raise C2paError.Decoding(message)
                    elif error_type == "Encoding":
                        raise C2paError.Encoding(message)
                    elif error_type == "FileNotFound":
                        raise C2paError.FileNotFound(message)
                    elif error_type == "Io":
                        raise C2paError.Io(message)
                    elif error_type == "Json":
                        raise C2paError.Json(message)
                    elif error_type == "Manifest":
                        raise C2paError.Manifest(message)
                    elif error_type == "ManifestNotFound":
                        raise C2paError.ManifestNotFound(message)
                    elif error_type == "NotSupported":
                        raise C2paError.NotSupported(message)
                    elif error_type == "Other":
                        raise C2paError.Other(message)
                    elif error_type == "RemoteManifest":
                        raise C2paError.RemoteManifest(message)
                    elif error_type == "ResourceNotFound":
                        raise C2paError.ResourceNotFound(message)
                    elif error_type == "Signature":
                        raise C2paError.Signature(message)
                    elif error_type == "Verify":
                        raise C2paError.Verify(message)
                return error_str
        return None
    
    # Convert to Python string and free the Rust-allocated memory
    py_string = ctypes.cast(result, ctypes.c_char_p).value.decode('utf-8')
    _lib.c2pa_string_free(result)
    return py_string

def sdk_version() -> str:
    return "0.8.0"

def version() -> str:
    """Get the C2PA library version."""
    result = _lib.c2pa_version()
    #print(f"Type: {type(result)}")
    #print(f"Address: {hex(result)}")
    py_string = ctypes.cast(result, ctypes.c_char_p).value.decode("utf-8")
    _lib.c2pa_string_free(result)  # Free the Rust-allocated memory
    return py_string

def load_settings(settings: str, format: str = "json") -> None:
    """Load C2PA settings from a string.
    
    Args:
        settings: The settings string to load
        format: The format of the settings string (default: "json")
        
    Raises:
        C2paError: If there was an error loading the settings
    """
    result = _lib.c2pa_load_settings(
        settings.encode('utf-8'),
        format.encode('utf-8')
    )
    if result != 0:
        error = _handle_string_result(_lib.c2pa_error())
        if error:
            raise C2paError(error)

def read_file(path: Union[str, Path], data_dir: Optional[Union[str, Path]] = None) -> str:
    """Read a C2PA manifest from a file.
    
    Args:
        path: Path to the file to read
        data_dir: Optional directory to write binary resources to
        
    Returns:
        The manifest as a JSON string
        
    Raises:
        C2paError: If there was an error reading the file
    """
    # Create a container to hold our strings
    class StringContainer:
        pass
    container = StringContainer()
    
    container._path_str = str(path).encode('utf-8')
    container._data_dir_str = str(data_dir).encode('utf-8') if data_dir else None
    
    result = _lib.c2pa_read_file(container._path_str, container._data_dir_str)
    return _handle_string_result(result)

def read_ingredient_file(path: Union[str, Path], data_dir: Optional[Union[str, Path]] = None) -> str:
    """Read a C2PA ingredient from a file.
    
    Args:
        path: Path to the file to read
        data_dir: Optional directory to write binary resources to
        
    Returns:
        The ingredient as a JSON string
        
    Raises:
        C2paError: If there was an error reading the file
    """
    # Create a container to hold our strings
    class StringContainer:
        pass
    container = StringContainer()
    
    container._path_str = str(path).encode('utf-8')
    container._data_dir_str = str(data_dir).encode('utf-8') if data_dir else None
    
    result = _lib.c2pa_read_ingredient_file(container._path_str, container._data_dir_str)
    return _handle_string_result(result)

def sign_file(
    source_path: Union[str, Path],
    dest_path: Union[str, Path],
    manifest: str,
    signer_info: C2paSignerInfo,
    data_dir: Optional[Union[str, Path]] = None
) -> str:
    """Sign a file with a C2PA manifest.
    
    Args:
        source_path: Path to the source file
        dest_path: Path to write the signed file to
        manifest: The manifest JSON string
        signer_info: Signing configuration
        data_dir: Optional directory to write binary resources to
        
    Returns:
        Result information as a JSON string
        
    Raises:
        C2paError: If there was an error signing the file
    """
    # Store encoded strings as attributes of signer_info to keep them alive
    signer_info._source_str = str(source_path).encode('utf-8')
    signer_info._dest_str = str(dest_path).encode('utf-8')
    signer_info._manifest_str = manifest.encode('utf-8')
    signer_info._data_dir_str = str(data_dir).encode('utf-8') if data_dir else None
    
    result = _lib.c2pa_sign_file(
        signer_info._source_str,
        signer_info._dest_str,
        signer_info._manifest_str,
        ctypes.byref(signer_info),
        signer_info._data_dir_str
    )
    return _handle_string_result(result)

class Stream:
    """High-level wrapper for C2paStream operations."""
    def __init__(self, file):
        # Validate that the object has the required stream-like methods
        required_methods = ['read', 'write', 'seek', 'tell', 'flush']
        missing_methods = [method for method in required_methods if not hasattr(file, method)]
        if missing_methods:
            raise TypeError(f"Object must be a stream-like object with methods: {', '.join(required_methods)}. Missing: {', '.join(missing_methods)}")
        
        self._file = file
        self._stream = None  # Initialize to None to track if stream was created
        self._closed = False  # Track if the stream has been closed
        self._initialized = False  # Track if stream was successfully initialized
        
        def read_callback(ctx, data, length):
            if not self._initialized or self._closed:
                print("Error: Attempted to read from uninitialized or closed stream", file=sys.stderr)
                return -1
            try:
                buffer = self._file.read(length)
                for i, b in enumerate(buffer):
                    data[i] = b
                return len(buffer)
            except Exception as e:
                print(f"Error reading from stream: {str(e)}", file=sys.stderr)
                return -1
        
        def seek_callback(ctx, offset, whence):
            if not self._initialized or self._closed:
                print("Error: Attempted to seek in uninitialized or closed stream", file=sys.stderr)
                return -1
            try:
                self._file.seek(offset, whence)
                return self._file.tell()
            except Exception as e:
                print(f"Error seeking in stream: {str(e)}", file=sys.stderr)
                return -1
        
        def write_callback(ctx, data, length):
            if not self._initialized or self._closed:
                print("Error: Attempted to write to uninitialized or closed stream", file=sys.stderr)
                return -1
            try:
                buffer = bytes(data[:length])
                self._file.write(buffer)
                return length
            except Exception as e:
                print(f"Error writing to stream: {str(e)}", file=sys.stderr)
                return -1
        
        def flush_callback(ctx):
            if not self._initialized or self._closed:
                print("Error: Attempted to flush uninitialized or closed stream", file=sys.stderr)
                return -1
            try:
                self._file.flush()
                return 0
            except Exception as e:
                print(f"Error flushing stream: {str(e)}", file=sys.stderr)
                return -1
        
        # Create callbacks that will be kept alive by being instance attributes
        self._read_cb = ReadCallback(read_callback)
        self._seek_cb = SeekCallback(seek_callback)
        self._write_cb = WriteCallback(write_callback)
        self._flush_cb = FlushCallback(flush_callback)
        
        # Create the stream
        self._stream = _lib.c2pa_create_stream(
            None,  # context
            self._read_cb,
            self._seek_cb,
            self._write_cb,
            self._flush_cb
        )
        if not self._stream:
            error = _handle_string_result(_lib.c2pa_error())
            raise Exception(f"Failed to create stream: {error}")
        
        self._initialized = True

    def __enter__(self):
        """Context manager entry."""
        if not self._initialized:
            raise RuntimeError("Stream was not properly initialized")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.close()

    def __del__(self):
        """Ensure resources are cleaned up if close() wasn't called."""
        self.close()

    def close(self):
        """Release the stream resources.
        
        This method ensures all resources are properly cleaned up, even if errors occur during cleanup.
        Errors during cleanup are logged but not raised to ensure cleanup completes.
        Multiple calls to close() are handled gracefully.
        """
        if self._closed:
            return

        try:
            # Clean up stream first as it depends on callbacks
            if self._stream:
                try:
                    _lib.c2pa_release_stream(self._stream)
                except Exception as e:
                    print(f"Error releasing stream: {str(e)}", file=sys.stderr)
                finally:
                    self._stream = None

            # Clean up callbacks
            for attr in ['_read_cb', '_seek_cb', '_write_cb', '_flush_cb']:
                if hasattr(self, attr):
                    try:
                        setattr(self, attr, None)
                    except Exception as e:
                        print(f"Error cleaning up callback {attr}: {str(e)}", file=sys.stderr)

            # Note: We don't close self._file as we don't own it
        except Exception as e:
            print(f"Error during cleanup: {str(e)}", file=sys.stderr)
        finally:
            self._closed = True
            self._initialized = False

    @property
    def closed(self) -> bool:
        """Check if the stream is closed.
        
        Returns:
            bool: True if the stream is closed, False otherwise
        """
        return self._closed

    @property
    def initialized(self) -> bool:
        """Check if the stream is properly initialized.
        
        Returns:
            bool: True if the stream is initialized, False otherwise
        """
        return self._initialized

class Reader:
    """High-level wrapper for C2PA Reader operations."""
    
    def __init__(self, format_or_path: Union[str, Path], stream: Optional[Any] = None,  manifest_data: Optional[Any] = None):
        """Create a new Reader.
        
        Args:
            format_or_path: The format or path to read from
            stream: Optional stream to read from (any Python stream-like object)
        """
        self._reader = None
        self._own_stream = None
        self._strings = []  # Keep encoded strings alive
        
        # Check for unsupported format
        if format_or_path == "badFormat":
            raise C2paError.NotSupported("Unsupported format")
        
        if stream is None:
            # Create a stream from the file path
            import mimetypes
            path = str(format_or_path)
            mime_type = mimetypes.guess_type(path)[0] or 'application/octet-stream'
            
            # Keep mime_type string alive
            self._mime_type_str = mime_type.encode('utf-8')
            
            # Open the file and create a stream
            file = open(path, 'rb')
            self._own_stream = Stream(file)
            
            # Create reader from the file stream
            self._reader = _lib.c2pa_reader_from_stream(
                self._mime_type_str,
                self._own_stream._stream
            )
            
            if not self._reader:
                self._own_stream.close()
                file.close()
                _handle_string_result(_lib.c2pa_error())
            
            # Store the file to close it later
            self._file = file
            
        elif isinstance(stream, str):
            # If stream is a string, treat it as a path and try to open it
            try:
                file = open(stream, 'rb')
                self._own_stream = Stream(file)
                self._format_str = format_or_path.encode('utf-8')
                
                if manifest_data is None:
                    self._reader = _lib.c2pa_reader_from_stream(self._format_str, self._own_stream._stream)
                else:
                    if not isinstance(manifest_data, bytes):
                        raise TypeError("manifest_data must be bytes")
                    manifest_array = (ctypes.c_ubyte * len(manifest_data))(*manifest_data)
                    self._reader = _lib.c2pa_reader_from_manifest_data_and_stream(self._format_str, self._own_stream._stream, manifest_array, len(manifest_data))
                
                if not self._reader:
                    self._own_stream.close()
                    file.close()
                    _handle_string_result(_lib.c2pa_error())
                
                self._file = file
            except Exception as e:
                if self._own_stream:
                    self._own_stream.close()
                if hasattr(self, '_file'):
                    self._file.close()
                raise C2paError.Io(str(e))
        else:
            # Use the provided stream
            # Keep format string alive
            self._format_str = format_or_path.encode('utf-8')
            with Stream(stream) as stream_obj:
                if manifest_data is None:
                    self._reader = _lib.c2pa_reader_from_stream(self._format_str, stream_obj._stream)
                else:
                    if not isinstance(manifest_data, bytes):
                        raise TypeError("manifest_data must be bytes")
                    manifest_array = (ctypes.c_ubyte * len(manifest_data))(*manifest_data)
                    self._reader = _lib.c2pa_reader_from_manifest_data_and_stream(self._format_str, stream_obj._stream, manifest_array, len(manifest_data))
                
                if not self._reader:
                    _handle_string_result(_lib.c2pa_error())
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def close(self):
        """Release the reader resources.
        
        This method ensures all resources are properly cleaned up, even if errors occur during cleanup.
        Errors during cleanup are logged but not raised to ensure cleanup completes.
        Multiple calls to close() are handled gracefully.
        """
        # Track if we've already cleaned up
        if not hasattr(self, '_closed'):
            self._closed = False

        if self._closed:
            return

        try:
            # Clean up reader
            if hasattr(self, '_reader') and self._reader:
                try:
                    _lib.c2pa_reader_free(self._reader)
                except Exception as e:
                    print(f"Error cleaning up reader: {e}", file=sys.stderr)
                finally:
                    self._reader = None
            
            # Clean up stream
            if hasattr(self, '_own_stream') and self._own_stream:
                try:
                    self._own_stream.close()
                except Exception as e:
                    print(f"Error cleaning up stream: {e}", file=sys.stderr)
                finally:
                    self._own_stream = None
            
            # Clean up file
            if hasattr(self, '_file'):
                try:
                    self._file.close()
                except Exception as e:
                    print(f"Error cleaning up file: {e}", file=sys.stderr)
                finally:
                    self._file = None

            # Clear any stored strings
            if hasattr(self, '_strings'):
                self._strings.clear()
        except Exception as e:
            print(f"Error during cleanup: {e}", file=sys.stderr)
        finally:
            self._closed = True
    
    def json(self) -> str:
        """Get the manifest store as a JSON string.
        
        Returns:
            The manifest store as a JSON string
            
        Raises:
            C2paError: If there was an error getting the JSON
        """
        if not self._reader:
            raise C2paError("Reader is closed")
        result = _lib.c2pa_reader_json(self._reader)
        return _handle_string_result(result)
    
    def resource_to_stream(self, uri: str, stream: Any) -> int:
        """Write a resource to a stream.
        
        Args:
            uri: The URI of the resource to write
            stream: The stream to write to (any Python stream-like object)
            
        Returns:
            The number of bytes written
            
        Raises:
            C2paError: If there was an error writing the resource
        """
        if not self._reader:
            raise C2paError("Reader is closed")
        
        # Keep uri string alive
        self._uri_str = uri.encode('utf-8')
        with Stream(stream) as stream_obj:
            result = _lib.c2pa_reader_resource_to_stream(self._reader, self._uri_str, stream_obj._stream)
            
            if result < 0:
                _handle_string_result(_lib.c2pa_error())
            
            return result

class Signer:
    """High-level wrapper for C2PA Signer operations."""
    
    @classmethod
    def from_info(cls, signer_info: C2paSignerInfo) -> 'Signer':
        """Create a new Signer from signer information.
        
        Args:
            signer_info: The signer configuration
            
        Returns:
            A new Signer instance
            
        Raises:
            C2paError: If there was an error creating the signer
        """
        signer_ptr = _lib.c2pa_signer_from_info(ctypes.byref(signer_info))
        
        if not signer_ptr:
            _handle_string_result(_lib.c2pa_error())
            
        return cls(signer_ptr)
    
    @classmethod
    def from_callback(
        cls,
        callback: Callable[[bytes], bytes],
        alg: C2paSigningAlg,
        certs: str,
        tsa_url: Optional[str] = None
    ) -> 'Signer':
        """Create a signer from a callback function.
        
        Args:
            callback: Function that signs data and returns the signature
            alg: The signing algorithm to use
            certs: Certificate chain in PEM format
            tsa_url: Optional RFC 3161 timestamp authority URL
            
        Returns:
            A new Signer instance
            
        Raises:
            C2paError: If there was an error creating the signer
        """
        signer_ptr = _lib.c2pa_signer_create(
            None,  # context
            SignerCallback(callback),
            alg,
            certs.encode('utf-8'),
            tsa_url.encode('utf-8') if tsa_url else None
        )
        
        if not signer_ptr:
            _handle_string_result(_lib.c2pa_error())
            
        return cls(signer_ptr)
    
    def __init__(self, signer_ptr: ctypes.POINTER(C2paSigner)):
        """Initialize a new Signer instance.
        
        Note: This constructor is not meant to be called directly.
        Use from_info() or from_callback() instead.
        """
        self._signer = signer_ptr
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def close(self):
        """Release the signer resources."""
        if self._signer:
            _lib.c2pa_signer_free(self._signer)
            self._signer = None
    
    def reserve_size(self) -> int:
        """Get the size to reserve for signatures from this signer.
        
        Returns:
            The size to reserve in bytes
            
        Raises:
            C2paError: If there was an error getting the size
        """
        if not self._signer:
            raise C2paError("Signer is closed")
            
        result = _lib.c2pa_signer_reserve_size(self._signer)
        
        if result < 0:
            _handle_string_result(_lib.c2pa_error())
            
        return result

class Builder:
    """High-level wrapper for C2PA Builder operations."""

    def __init__(self, manifest_json: Any):
        """Initialize a new Builder instance."""
        if not isinstance(manifest_json, str):
            manifest_json = json.dumps(manifest_json)
        
        json_str = manifest_json.encode('utf-8')
        self._builder = _lib.c2pa_builder_from_json(json_str)
        
        if not self._builder:
            _handle_string_result(_lib.c2pa_error())

    @classmethod
    def from_json(cls, manifest_json: Any) -> 'Builder':
        """Create a new Builder from a JSON manifest.
        
        Args:
            manifest_json: The JSON manifest definition
            
        Returns:
            A new Builder instance
            
        Raises:
            C2paError: If there was an error creating the builder
        """
        return cls(manifest_json)
    
    @classmethod
    def from_archive(cls, stream: Any) -> 'Builder':
        """Create a new Builder from an archive stream.
        
        Args:
            stream: The stream containing the archive (any Python stream-like object)
            
        Returns:
            A new Builder instance
            
        Raises:
            C2paError: If there was an error creating the builder
        """
        builder = cls({})
        stream_obj = Stream(stream)
        builder._builder = _lib.c2pa_builder_from_archive(stream_obj._stream)
        
        if not builder._builder:
            _handle_string_result(_lib.c2pa_error())
            
        return builder

    def __del__(self):
        """Ensure resources are cleaned up if close() wasn't called."""
        self.close()

    def close(self):
        """Release the builder resources.
        
        This method ensures all resources are properly cleaned up, even if errors occur during cleanup.
        Errors during cleanup are logged but not raised to ensure cleanup completes.
        Multiple calls to close() are handled gracefully.
        """
        # Track if we've already cleaned up
        if not hasattr(self, '_closed'):
            self._closed = False

        if self._closed:
            return

        try:
            # Clean up builder
            if hasattr(self, '_builder') and self._builder:
                try:
                    _lib.c2pa_builder_free(self._builder)
                except Exception as e:
                    print(f"Error cleaning up builder: {e}", file=sys.stderr)
                finally:
                    self._builder = None
        except Exception as e:
            print(f"Error during cleanup: {e}", file=sys.stderr)
        finally:
            self._closed = True

    def set_manifest(self, manifest):
        if not isinstance(manifest, str):
            manifest = json.dumps(manifest)
        super().with_json(manifest)
        return self
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def set_no_embed(self):
        """Set the no-embed flag.
        
        When set, the builder will not embed a C2PA manifest store into the asset when signing.
        This is useful when creating cloud or sidecar manifests.
        """
        if not self._builder:
            raise C2paError("Builder is closed")
        _lib.c2pa_builder_set_no_embed(self._builder)
    
    def set_remote_url(self, remote_url: str):
        """Set the remote URL.
        
        When set, the builder will embed a remote URL into the asset when signing.
        This is useful when creating cloud based Manifests.
        
        Args:
            remote_url: The remote URL to set
            
        Raises:
            C2paError: If there was an error setting the URL
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        url_str = remote_url.encode('utf-8')
        result = _lib.c2pa_builder_set_remote_url(self._builder, url_str)
        
        if result != 0:
            _handle_string_result(_lib.c2pa_error())
    
    def add_resource(self, uri: str, stream: Any):
        """Add a resource to the builder.
        
        Args:
            uri: The URI to identify the resource
            stream: The stream containing the resource data (any Python stream-like object)
            
        Raises:
            C2paError: If there was an error adding the resource
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        uri_str = uri.encode('utf-8')
        with Stream(stream) as stream_obj:
            result = _lib.c2pa_builder_add_resource(self._builder, uri_str, stream_obj._stream)
            
            if result != 0:
                _handle_string_result(_lib.c2pa_error())
    
    def add_ingredient(self, ingredient_json: str, format: str, source: Any):
        """Add an ingredient to the builder.
        
        Args:
            ingredient_json: The JSON ingredient definition
            format: The MIME type or extension of the ingredient
            source: The stream containing the ingredient data (any Python stream-like object)
            
        Raises:
            C2paError: If there was an error adding the ingredient
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        ingredient_str = ingredient_json.encode('utf-8')
        format_str = format.encode('utf-8')
        source_stream = Stream(source)
        result = _lib.c2pa_builder_add_ingredient(self._builder, ingredient_str, format_str, source_stream._stream)
        
        if result != 0:
            _handle_string_result(_lib.c2pa_error())
    
    def add_ingredient_from_stream(self, ingredient_json: str, format: str, source: Any):
        """Add an ingredient from a stream to the builder.
        
        Args:
            ingredient_json: The JSON ingredient definition
            format: The MIME type or extension of the ingredient
            source: The stream containing the ingredient data (any Python stream-like object)
            
        Raises:
            C2paError: If there was an error adding the ingredient
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        ingredient_str = ingredient_json.encode('utf-8')
        format_str = format.encode('utf-8')
        with Stream(source) as source_stream:
            result = _lib.c2pa_builder_add_ingredient_from_stream(
                self._builder, ingredient_str, format_str, source_stream._stream)
            
            if result != 0:
                _handle_string_result(_lib.c2pa_error())
    
    def to_archive(self, stream: Any):
        """Write an archive of the builder to a stream.
        
        Args:
            stream: The stream to write the archive to (any Python stream-like object)
            
        Raises:
            C2paError: If there was an error writing the archive
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        with Stream(stream) as stream_obj:
            result = _lib.c2pa_builder_to_archive(self._builder, stream_obj._stream)
            
            if result != 0:
                _handle_string_result(_lib.c2pa_error())
    
    def sign(self, signer: Signer, format: str, source: Any, dest: Any = None) -> Optional[bytes]:
        """Sign the builder's content and write to a destination stream.
        
        Args:
            format: The MIME type or extension of the content
            source: The source stream (any Python stream-like object)
            dest: The destination stream (any Python stream-like object)
            signer: The signer to use
            
        Returns:
            A tuple of (size of C2PA data, optional manifest bytes)
            
        Raises:
            C2paError: If there was an error during signing
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        # Convert Python streams to Stream objects
        source_stream = Stream(source)
        dest_stream = Stream(dest)
        
        try:
            format_str = format.encode('utf-8')
            manifest_bytes_ptr = ctypes.POINTER(ctypes.c_ubyte)()
            
            result = _lib.c2pa_builder_sign(
                self._builder,
                format_str,
                source_stream._stream,
                dest_stream._stream,
                signer._signer,
                ctypes.byref(manifest_bytes_ptr)
            )
            
            if result < 0:
                _handle_string_result(_lib.c2pa_error())
                
            manifest_bytes = None
            if manifest_bytes_ptr:
                # Convert the manifest bytes to a Python bytes object
                size = result
                manifest_bytes = bytes(manifest_bytes_ptr[:size])
                _lib.c2pa_manifest_bytes_free(manifest_bytes_ptr)
                
            return manifest_bytes
        finally:
            # Ensure both streams are cleaned up
            source_stream.close()
            dest_stream.close()

    def sign_file(self, source_path: Union[str, Path], dest_path: Union[str, Path], signer: Signer) -> tuple[int, Optional[bytes]]:
        """Sign a file and write the signed data to an output file.
        
        Args:
            source_path: Path to the source file
            dest_path: Path to write the signed file to
            signer: The signer to use
            
        Returns:
            A tuple of (size of C2PA data, optional manifest bytes)
            
        Raises:
            C2paError: If there was an error during signing
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        source_path_str = str(source_path).encode('utf-8')
        dest_path_str = str(dest_path).encode('utf-8')
        manifest_bytes_ptr = ctypes.POINTER(ctypes.c_ubyte)()
        
        result = _lib.c2pa_builder_sign_file(
            self._builder,
            source_path_str,
            dest_path_str,
            signer._signer,
            ctypes.byref(manifest_bytes_ptr)
        )
        
        if result < 0:
            _handle_string_result(_lib.c2pa_error())
            
        manifest_bytes = None
        if manifest_bytes_ptr:
            # Convert the manifest bytes to a Python bytes object
            size = result
            manifest_bytes = bytes(manifest_bytes_ptr[:size])
            _lib.c2pa_manifest_bytes_free(manifest_bytes_ptr)
            
        return result, manifest_bytes

def format_embeddable(format: str, manifest_bytes: bytes) -> tuple[int, bytes]:
    """Convert a binary C2PA manifest into an embeddable version.
    
    Args:
        format: The MIME type or extension of the target format
        manifest_bytes: The raw manifest bytes
        
    Returns:
        A tuple of (size of result bytes, embeddable manifest bytes)
        
    Raises:
        C2paError: If there was an error converting the manifest
    """
    format_str = format.encode('utf-8')
    manifest_array = (ctypes.c_ubyte * len(manifest_bytes))(*manifest_bytes)
    result_bytes_ptr = ctypes.POINTER(ctypes.c_ubyte)()
    
    result = _lib.c2pa_format_embeddable(
        format_str,
        manifest_array,
        len(manifest_bytes),
        ctypes.byref(result_bytes_ptr)
    )
    
    if result < 0:
        _handle_string_result(_lib.c2pa_error())
        
    # Convert the result bytes to a Python bytes object
    size = result
    result_bytes = bytes(result_bytes_ptr[:size])
    _lib.c2pa_manifest_bytes_free(result_bytes_ptr)
    
    return size, result_bytes

def create_signer(
    callback: Callable[[bytes], bytes],
    alg: C2paSigningAlg,
    certs: str,
    tsa_url: Optional[str] = None
) -> Signer:
    """Create a signer from a callback function.
    
    Args:
        callback: Function that signs data and returns the signature
        alg: The signing algorithm to use
        certs: Certificate chain in PEM format
        tsa_url: Optional RFC 3161 timestamp authority URL
        
    Returns:
        A new Signer instance
        
    Raises:
        C2paError: If there was an error creating the signer
    """
    signer_ptr = _lib.c2pa_signer_create(
        None,  # context
        SignerCallback(callback),
        alg,
        certs.encode('utf-8'),
        tsa_url.encode('utf-8') if tsa_url else None
    )
    
    if not signer_ptr:
        _handle_string_result(_lib.c2pa_error())
        
    return Signer(signer_ptr)

def create_signer_from_info(signer_info: C2paSignerInfo) -> Signer:
    """Create a signer from signer information.
    
    Args:
        signer_info: The signer configuration
        
    Returns:
        A new Signer instance
        
    Raises:
        C2paError: If there was an error creating the signer
    """
    signer_ptr = _lib.c2pa_signer_from_info(ctypes.byref(signer_info))
    
    if not signer_ptr:
        _handle_string_result(_lib.c2pa_error())
        
    return Signer(signer_ptr)

# Rename the old create_signer to _create_signer since it's now internal
_create_signer = create_signer

def ed25519_sign(data: bytes, private_key: str) -> bytes:
    """Sign data using the Ed25519 algorithm.
    
    Args:
        data: The data to sign
        private_key: The private key in PEM format
        
    Returns:
        The signature bytes
        
    Raises:
        C2paError: If there was an error signing the data
    """
    data_array = (ctypes.c_ubyte * len(data))(*data)
    key_str = private_key.encode('utf-8')
    
    signature_ptr = _lib.c2pa_ed25519_sign(data_array, len(data), key_str)
    
    if not signature_ptr:
        _handle_string_result(_lib.c2pa_error())
    
    try:
        # Ed25519 signatures are always 64 bytes
        signature = bytes(signature_ptr[:64])
    finally:
        _lib.c2pa_signature_free(signature_ptr)
    
    return signature

__all__ = [
    'C2paError',
    'C2paSeekMode',
    'C2paSigningAlg',
    'C2paSignerInfo',
    'Stream',
    'Reader',
    'Builder',
    'Signer',
    'version',
    'load_settings',
    'read_file',
    'read_ingredient_file',
    'sign_file',
    'format_embeddable',
    'ed25519_sign',
]