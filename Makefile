OS := $(shell uname)
CFLAGS = -I. -Wall
ENV =
ifeq ($(findstring _NT, $(OS)), _NT)
CFLAGS += -L./target/release -lc2pa_c
CC := gcc
CXX := g++
ENV = LD_LIBRARY_PATH=target/release
endif
ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif
ifeq ($(OS), Linux)
CFLAGS = -pthread -Wl,--no-as-needed -ldl -lm
ENV = LD_LIBRARY_PATH=target/release
endif

show:
	@echo $(OS)
	@echo $(CFLAGS)
	@echo $(CC)

BUILD_DIR = target/cmake

check-format:
	cargo fmt -- --check

clippy:
	cargo clippy --all-features --all-targets -- -D warnings

test-rust:
	cargo test --release
 
cmake:
	mkdir -p $(BUILD_DIR)
	cmake -S./ -B./$(BUILD_DIR) -G "Ninja"

release:
	cargo build --release
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

test-c: release
	$(CC) $(CFLAGS) tests/test.c -o target/release/ctest -lc2pa_c -L./target/release
	$(ENV) target/release/ctest

test-cpp: release cmake
	cd $(BUILD_DIR); ninja;
	ls -lah $(BUILD_DIR)/tests
	cd $(BUILD_DIR)/tests; ./unit_tests

demo: cmake release
	cmake --build ./$(BUILD_DIR) --target demo
	cd $(BUILD_DIR); examples/demo

training: cmake release
	cmake --build ./$(BUILD_DIR) --target training
	cd $(BUILD_DIR); examples/training

examples: training demo

# Creates a folder wtih library, samples and readme
package:
	rm -rf target/c2pa-c
	mkdir -p target/c2pa-c
	mkdir -p target/c2pa-c/include
	cp target/release/libc2pa_c.so target/c2pa-c/libc2pa_c.so
	cp README.md target/c2pa-c/README.md
	cp include/* target/c2pa-c/include

test: test-rust test-cpp 

all: unit-tests examples
