OS := $(shell uname)
CFLAGS = -I. -Wall 
ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif
ifeq ($(OS), Linux)
CFLAGS = -pthread -Wl,--no-as-needed -ldl -lm
endif

RUSTFLAGS = -Ctarget-feature=-crt-static

generate-bindings:
	cargo install cbindgen
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

build:
	RUSTFLAGS=$(RUSTFLAGS) cargo build --release --target $(TARGET)
	$(MAKE) generate-bindings

build-cross:
	cross build --release --target $(TARGET)
	$(MAKE) generate-bindings

release: 
	cargo build --release
	$(MAKE) generate-bindings

test-c:
	$(CC) $(CFLAGS) tests/test.c -o target/ctest -lc2pa_c -L./target/$(TARGET)/release
	target/ctest

test-cpp:
	g++ $(CFLAGS) -std=c++11 tests/test.cpp -o target/cpptest -lc2pa_c -L./target/$(TARGET)/release 
	target/cpptest

example:
	g++ $(CFLAGS) -std=c++17 examples/training.cpp -o target/training -lc2pa_c -L./target/$(TARGET)/release
	target/training

# Creates a folder wtih c2patool bin, samples and readme
package:
	rm -rf target/c2pa-c
	mkdir -p target/c2pa-c
	mkdir -p target/c2pa-c/include
	cp target/release/libc2pa_c.dylib target/c2pa-c/libc2pa_c.dylib
	cp README.md target/c2pa-c/README.md
	cp include/* target/c2pa-c/include

test: test-c test-cpp