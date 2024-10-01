from cffi import FFI
ffibuilder = FFI()

# cdef() expects a single string declaring the C types, functions and
# globals needed to use the shared object. It must be in valid C syntax.
ffibuilder.cdef("""
    char *c2pa_version(void);
""")

# set_source() gives the name of the python extension module to
# produce, and some C source code as a string.  This C code needs
# to make the declarated functions, types and globals available,
# so it is often just the "#include".
ffibuilder.set_source("_c2pa_cffi",
"""
     #include "include/c2pa.h"   // the C header of the library
""",
     libraries=['c2pa_c'],   # library name, for the linker
     library_dirs=["/Users/gpeacock/dev/c2pa-c/target/release"], # where to find the library) 
     output_dir="target/cffi_build") # set the output folder

if __name__ == "__main__":
    ffibuilder.compile(verbose=True)