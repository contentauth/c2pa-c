OS := $(shell uname)
CFLAGS = -I. -Wall
ENV =
ifeq ($(findstring _NT, $(OS)), _NT)
CFLAGS += -L./target/release -lc2pa_c
CC := gcc
CXX := g++
ENV = PATH="$(shell pwd)/target/release:$(PATH)"
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
	cd $(BUILD_DIR) && ninja
	mkdir -p $(BUILD_DIR)/src/tests/fixtures
	cp -r tests/fixtures/* $(BUILD_DIR)/src/tests/fixtures/
ifeq ($(findstring _NT, $(OS)), _NT)
	@echo "Current directory: $$(pwd)"
	@echo "DLL location: $$(pwd)/target/release/c2pa_c.dll"
	@echo "Test exe location: $$(pwd)/$(BUILD_DIR)/src/c2pa_c_tests.exe"
	cp target/release/c2pa_c.dll $(BUILD_DIR)/src/
	cd $(BUILD_DIR)/src && $(ENV) cmd /c "set PATH=$$(pwd);%PATH% && c2pa_c_tests.exe"
else
	cd $(BUILD_DIR)/src && $(ENV) ./c2pa_c_tests
endif

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

all: test examples
