OS := $(shell uname)

BUILD_DIR = build

cmake:
	cmake -S . -B $(BUILD_DIR) -G "Ninja"

test-c: cmake
	cmake --build $(BUILD_DIR) --target ctest
	$(BUILD_DIR)/tests/ctest

test-cpp: cmake
	cmake --build $(BUILD_DIR) --target c2pa_c_tests
	$(BUILD_DIR)/tests/c2pa_c_tests

demo: cmake
	cmake --build $(BUILD_DIR) --target demo
	$(BUILD_DIR)/examples/demo

training: cmake
	cmake --build $(BUILD_DIR) --target training
	$(BUILD_DIR)/examples/training

examples: training demo

test: test-c test-cpp

all: test examples

clean:
	rm -rf $(BUILD_DIR)

