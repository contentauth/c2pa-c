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

check-format:
	cargo fmt -- --check

clippy:
	cargo clippy --all-features --all-targets -- -D warnings

test-rust:
	cargo test --
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

unit-tests: test-rust
	mkdir -p target/cmake
	cmake -S./ -B./target/cmake -G "Ninja"
	cmake --build ./target/cmake --target unit_tests
	cd target/cmake; ./unit_tests

release:
	cargo build --release
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

test-c: release
	$(CC) $(CFLAGS) tests/test.c -o target/ctest -lc2pa_c -L./target/release
	$(ENV) target/ctest

test-cpp: release
	g++ $(CFLAGS) -std=c++17 tests/test.cpp -o target/cpptest -lc2pa_c -L./target/release 
	$(ENV) target/cpptest

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