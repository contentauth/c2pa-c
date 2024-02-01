OS := $(shell uname)
CFLAGS = -I. -Wall
ENV =
ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif
ifeq ($(OS), Linux)
CFLAGS = -pthread -Wl,--no-as-needed -ldl -lm
ENV = LD_LIBRARY_PATH=target/release
endif

# This is to enable building dynamic libraries with musl
RUSTFLAGS = -Ctarget-feature=-crt-static

generate-bindings:
	cargo install cbindgen
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

build:
	RUSTFLAGS=$(RUSTFLAGS) cargo build --release --target $(TARGET)
	$(MAKE) generate-bindings

build-cross:
	RUSTFLAGS=$(RUSTFLAGS) cross build --release --target $(TARGET)
	$(MAKE) generate-bindings

release: 
	cargo build --release
	$(MAKE) generate-bindings

check-format:
	cargo fmt -- --check

clippy:
	cargo clippy --all-features --all-targets -- -D warnings

test-rust:
	cargo test --all-features

release: 
	cargo build --release --target $(TARGET)
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

test-c:
	$(CC) $(CFLAGS) tests/test.c -o target/$(TARGET)/ctest -lc2pa_c -L./target/$(TARGET)/release
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:./target/$(TARGET)/release target/$(TARGET)/ctest

test-cpp:
	g++ $(CFLAGS) -std=c++11 tests/test.cpp -o target/$(TARGET)/cpptest -lc2pa_c -L./target/$(TARGET)/release 
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:./target/$(TARGET)/release target/$(TARGET)/cpptest

example: release
	g++ $(CFLAGS) -std=c++17 examples/training.cpp -o target/training -lc2pa_c -L./target/release
	$(ENV) target/training

# Creates a folder wtih c2patool bin, samples and readme
package:
	rm -rf target/c2pa-c
	mkdir -p target/c2pa-c
	mkdir -p target/c2pa-c/include
	cp target/release/libc2pa_c.so target/c2pa-c/libc2pa_c.so
	cp README.md target/c2pa-c/README.md
	cp include/* target/c2pa-c/include

test: check-format clippy test-rust test-c test-cpp

all: test example
