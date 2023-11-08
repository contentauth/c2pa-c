OS := $(shell uname)
CFLAGS = -I. -Wall 
ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif
ifeq ($(OS), Linux)
CFLAGS = -pthread -Wl,--no-as-needed -ldl -lm
endif

check-format:
	cargo +nightly fmt -- --check

clippy:
	cargo clippy --all-features --all-targets -- -D warnings

test-rust:
	cargo test --all-features

release: 
	cargo build --release
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

test-c: release
	$(CC) $(CFLAGS) tests/test.c -o target/ctest -lc2pa_c -L./target/release
	target/ctest

test-cpp: release
	g++ $(CFLAGS) -std=c++17 tests/test.cpp -o target/cpptest -lc2pa_c -L./target/release 
	target/cpptest

example: release
	g++ $(CFLAGS) -std=c++17 examples/training.cpp -o target/training -lc2pa_c -L./target/release
	target/training

# Creates a folder wtih c2patool bin, samples and readme
package:
	rm -rf target/c2pa-c
	mkdir -p target/c2pa-c
	mkdir -p target/c2pa-c/include
	cp target/release/libc2pa_c.dylib target/c2pa-c/libc2pa_c.dylib
	cp README.md target/c2pa-c/README.md
	cp include/* target/c2pa-c/include

test: check-format clippy test-rust test-c test-cpp

all: test example