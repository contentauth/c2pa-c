import ctypes
import enum
import os
import sys
from pathlib import Path
from typing import Optional, Union, Callable

# Determine the library name based on the platform
if sys.platform == "win32":
    _lib_name = "c2pa_c.dll"
elif sys.platform == "darwin":
    _lib_name = "libc2pa_c.dylib"
else:
    _lib_name = "libc2pa_c.so"

# Try to find the library in common locations
_lib_paths = [
    Path(__file__).parent / _lib_name,  # Same directory as this file
    Path(__file__).parent / "target" / "release" / _lib_name,  # target/release
    Path(__file__).parent.parent / "target" / "release" / _lib_name,  # ../target/release
]

for _path in _lib_paths:
    if _path.exists():
        _lib = ctypes.CDLL(str(_path))
        break
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

def _handle_string_result(result: ctypes.c_void_p, check_error: bool = True) -> Optional[str]:
    """Helper function to handle string results from C2PA functions."""
    if not result:  # NULL pointer
        if check_error:
            error = _lib.c2pa_error()
            if error:
                error_str = ctypes.cast(error, ctypes.c_char_p).value.decode('utf-8')
                _lib.c2pa_string_free(error)
                raise C2paError(error_str)
        return None
    
    # Convert to Python string and free the Rust-allocated memory
    py_string = ctypes.cast(result, ctypes.c_char_p).value.decode('utf-8')
    _lib.c2pa_string_free(result)
    return py_string

class C2paError(Exception):
    """Exception raised for C2PA errors."""
    pass

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



# Helper class for stream operations
class Stream:
    """High-level wrapper for C2paStream operations."""
    def __init__(self, file):
        self._file = file
        
        def read_callback(ctx, data, length):
            try:
                buffer = self._file.read(length)
                for i, b in enumerate(buffer):
                    data[i] = b
                return len(buffer)
            except Exception:
                return -1
        
        def seek_callback(ctx, offset, whence):
            try:
                self._file.seek(offset, whence)
                return self._file.tell()
            except Exception:
                return -1
        
        def write_callback(ctx, data, length):
            try:
                buffer = bytes(data[:length])
                self._file.write(buffer)
                return length
            except Exception:
                return -1
        
        def flush_callback(ctx):
            try:
                self._file.flush()
                return 0
            except Exception:
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
            raise Exception("Failed to create stream")

    def __del__(self):
        if hasattr(self, '_stream') and self._stream:
            _lib.c2pa_release_stream(self._stream)

class Reader:
    """High-level wrapper for C2PA Reader operations."""
    
    def __init__(self, format_or_path: Union[str, Path], stream: Optional[Stream] = None):
        """Create a new Reader."""
        self._reader = None
        self._own_stream = None
        self._strings = []  # Keep encoded strings alive
        
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
            
            def read_callback(ctx, data, length):
                try:
                    buffer = file.read(length)
                    for i, b in enumerate(buffer):
                        data[i] = b
                    return len(buffer)
                except Exception:
                    return -1
            
            def seek_callback(ctx, offset, whence):
                try:
                    file.seek(offset, whence)
                    return file.tell()
                except Exception:
                    return -1
            
            def write_callback(ctx, data, length):
                return -1  # Read-only
                
            def flush_callback(ctx):
                return 0  # No-op for read-only
            
            # Create callbacks that will be kept alive by being instance attributes
            self._read_cb = ReadCallback(read_callback)
            self._seek_cb = SeekCallback(seek_callback)
            self._write_cb = WriteCallback(write_callback)
            self._flush_cb = FlushCallback(flush_callback)
            
            # Create the stream
            self._own_stream._stream = _lib.c2pa_create_stream(
                None,
                self._read_cb,
                self._seek_cb,
                self._write_cb,
                self._flush_cb
            )
            
            if not self._own_stream._stream:
                file.close()
                error = _handle_string_result(_lib.c2pa_error())
                raise C2paError(error)
            
            # Create reader from the file stream
            self._reader = _lib.c2pa_reader_from_stream(
                self._mime_type_str,
                self._own_stream._stream
            )
            
            if not self._reader:
                self._own_stream.close()
                file.close()
                error = _handle_string_result(_lib.c2pa_error())
                raise C2paError(error)
            
            # Store the file to close it later
            self._file = file
            
        else:
            # Use the provided stream
            # Keep format string alive
            self._format_str = format_or_path.encode('utf-8')
            self._reader = _lib.c2pa_reader_from_stream(self._format_str, stream._stream)
            
            if not self._reader:
                error = _handle_string_result(_lib.c2pa_error())
                raise C2paError(error)
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def close(self):
        """Release the reader resources."""
        if self._reader:
            _lib.c2pa_reader_free(self._reader)
            self._reader = None
        
        if hasattr(self, '_own_stream') and self._own_stream:
            self._own_stream.close()
            self._own_stream = None
            
        if hasattr(self, '_file'):
            self._file.close()
            del self._file
    
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
    
    def resource_to_stream(self, uri: str, stream: Stream) -> int:
        """Write a resource to a stream."""
        if not self._reader:
            raise C2paError("Reader is closed")
        
        # Keep uri string alive
        self._uri_str = uri.encode('utf-8')
        result = _lib.c2pa_reader_resource_to_stream(self._reader, self._uri_str, stream._stream)
        
        if result < 0:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
        
        return result

class Signer:
    """High-level wrapper for C2PA Signer operations."""
    
    def __init__(self, signer_ptr: ctypes.POINTER(C2paSigner)):
        """Initialize a new Signer instance.
        
        Note: This constructor is not meant to be called directly.
        Use create_signer() or create_signer_from_info() instead.
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
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
            
        return result

class Builder:
    """High-level wrapper for C2PA Builder operations."""
    
    @classmethod
    def from_json(cls, manifest_json: str) -> 'Builder':
        """Create a new Builder from a JSON manifest.
        
        Args:
            manifest_json: The JSON manifest definition
            
        Returns:
            A new Builder instance
            
        Raises:
            C2paError: If there was an error creating the builder
        """
        builder = cls()
        json_str = manifest_json.encode('utf-8')
        builder._builder = _lib.c2pa_builder_from_json(json_str)
        
        if not builder._builder:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
            
        return builder
    
    @classmethod
    def from_archive(cls, stream: Stream) -> 'Builder':
        """Create a new Builder from an archive stream.
        
        Args:
            stream: The stream containing the archive
            
        Returns:
            A new Builder instance
            
        Raises:
            C2paError: If there was an error creating the builder
        """
        builder = cls()
        builder._builder = _lib.c2pa_builder_from_archive(stream._stream)
        
        if not builder._builder:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
            
        return builder
    
    def __init__(self):
        """Initialize a new Builder instance."""
        self._builder = None
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
    
    def close(self):
        """Release the builder resources."""
        if self._builder:
            _lib.c2pa_builder_free(self._builder)
            self._builder = None
    
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
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
    
    def add_resource(self, uri: str, stream: Stream):
        """Add a resource to the builder.
        
        Args:
            uri: The URI to identify the resource
            stream: The stream containing the resource data
            
        Raises:
            C2paError: If there was an error adding the resource
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        uri_str = uri.encode('utf-8')
        result = _lib.c2pa_builder_add_resource(self._builder, uri_str, stream._stream)
        
        if result != 0:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
    
    def add_ingredient(self, ingredient_json: str, format: str, source: Stream):
        """Add an ingredient to the builder.
        
        Args:
            ingredient_json: The JSON ingredient definition
            format: The MIME type or extension of the ingredient
            source: The stream containing the ingredient data
            
        Raises:
            C2paError: If there was an error adding the ingredient
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        ingredient_str = ingredient_json.encode('utf-8')
        format_str = format.encode('utf-8')
        result = _lib.c2pa_builder_add_ingredient(self._builder, ingredient_str, format_str, source._stream)
        
        if result != 0:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
    
    def add_ingredient_from_stream(self, ingredient_json: str, format: str, source: Stream):
        """Add an ingredient from a stream to the builder.
        
        Args:
            ingredient_json: The JSON ingredient definition
            format: The MIME type or extension of the ingredient
            source: The stream containing the ingredient data
            
        Raises:
            C2paError: If there was an error adding the ingredient
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        ingredient_str = ingredient_json.encode('utf-8')
        format_str = format.encode('utf-8')
        result = _lib.c2pa_builder_add_ingredient_from_stream(
            self._builder, ingredient_str, format_str, source._stream)
        
        if result != 0:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
    
    def to_archive(self, stream: Stream):
        """Write an archive of the builder to a stream.
        
        Args:
            stream: The stream to write the archive to
            
        Raises:
            C2paError: If there was an error writing the archive
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        result = _lib.c2pa_builder_to_archive(self._builder, stream._stream)
        
        if result != 0:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
    
    def sign(self, format: str, source: Stream, dest: Stream, signer: Signer) -> tuple[int, Optional[bytes]]:
        """Sign the builder's content and write to a destination stream.
        
        Args:
            format: The MIME type or extension of the content
            source: The stream containing the source data
            dest: The stream to write the signed data to
            signer: The signer to use
            
        Returns:
            A tuple of (size of C2PA data, optional manifest bytes)
            
        Raises:
            C2paError: If there was an error during signing
        """
        if not self._builder:
            raise C2paError("Builder is closed")
            
        format_str = format.encode('utf-8')
        manifest_bytes_ptr = ctypes.POINTER(ctypes.c_ubyte)()
        
        result = _lib.c2pa_builder_sign(
            self._builder,
            format_str,
            source._stream,
            dest._stream,
            signer._signer,
            ctypes.byref(manifest_bytes_ptr)
        )
        
        if result < 0:
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
            
        manifest_bytes = None
        if manifest_bytes_ptr:
            # Convert the manifest bytes to a Python bytes object
            size = result
            manifest_bytes = bytes(manifest_bytes_ptr[:size])
            _lib.c2pa_manifest_bytes_free(manifest_bytes_ptr)
            
        return result, manifest_bytes

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
            error = _handle_string_result(_lib.c2pa_error())
            raise C2paError(error)
            
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
        error = _handle_string_result(_lib.c2pa_error())
        raise C2paError(error)
        
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
        error = _handle_string_result(_lib.c2pa_error())
        raise C2paError(error)
        
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
        error = _handle_string_result(_lib.c2pa_error())
        raise C2paError(error)
        
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
        error = _handle_string_result(_lib.c2pa_error())
        raise C2paError(error)
    
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
    'create_signer',
    'create_signer_from_info',
    'ed25519_sign',
] 