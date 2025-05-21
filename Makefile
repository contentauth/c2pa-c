OS := $(shell uname)

BUILD_DIR = build

cmake:
	cmake -S . -B $(BUILD_DIR) -G "Ninja"
	cmake --build $(BUILD_DIR)

test: cmake
	cd $(BUILD_DIR) && ctest --output-on-failure

demo: cmake
	cmake --build $(BUILD_DIR) --target demo
	$(BUILD_DIR)/examples/demo

training: cmake
	cmake --build $(BUILD_DIR) --target training
	$(BUILD_DIR)/examples/training

examples: training demo

all: test examples

clean:
	rm -rf $(BUILD_DIR)

