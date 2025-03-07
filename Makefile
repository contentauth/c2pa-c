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
 
cmake_clean:
	rm -rf $(BUILD_DIR)

# Set default build command and generator
BUILD_CMD := ninja
CMAKE_GEN := Ninja

# Override for Windows if needed
ifeq ($(findstring _NT, $(OS)), _NT)
    CMAKE_GEN := "MinGW Makefiles"
    BUILD_CMD := mingw32-make
endif

cmake: cmake_clean
	mkdir -p $(BUILD_DIR)
	cmake -S./ -B./$(BUILD_DIR) -G $(CMAKE_GEN)

release:
	cargo build --release
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

test-c: release
	$(CC) $(CFLAGS) tests/test.c -o target/release/ctest -lc2pa_c -L./target/release
	$(ENV) target/release/ctest

test-cpp: release cmake
	cd $(BUILD_DIR); $(BUILD_CMD);
	cd $(BUILD_DIR); ls -lah src
	cd $(BUILD_DIR); ./src/c2pa_c_tests

demo: cmake release
	cd $(BUILD_DIR); $(BUILD_CMD) examples/demo
	cd $(BUILD_DIR); examples/demo

training: cmake release
	cd $(BUILD_DIR); $(BUILD_CMD) examples/training
	cd $(BUILD_DIR); examples/training

examples: training demo

# Creates a folder with library, samples and readme
package: release
	rm -rf target/c2pa-c
	mkdir -p target/c2pa-c
	mkdir -p target/c2pa-c/include
	mkdir -p target/c2pa-c/lib
	mkdir -p target/c2pa-c/src
ifeq ($(findstring _NT, $(OS)), _NT)
	cp target/release/c2pa_c.dll target/c2pa-c/lib/
	cp target/release/c2pa_c.dll.lib target/c2pa-c/lib/
else ifeq ($(OS), Darwin)
	cp target/release/libc2pa_c.dylib target/c2pa-c/lib/
else
	cp target/release/libc2pa_c.so target/c2pa-c/lib/
endif
	cp -r examples target/c2pa-c/
	cp -r src/*.cpp src/*.hpp target/c2pa-c/src/
	cp README.md target/c2pa-c/
	cp -r include/* target/c2pa-c/include/

test: test-rust test-cpp 

all: test-c test-cpp examples

# Docker targets
docker-build:
	docker build -t c2pa-c-build .

docker-run:
	docker run -it --rm -v $(PWD):/app c2pa-c-build
