OS := $(shell uname)

# Build directories
BUILD_DIR = build
DEBUG_BUILD_DIR = build/debug
RELEASE_BUILD_DIR = build/release

# Default target
all: test examples

# Debug build
debug:
	cmake -S . -B $(DEBUG_BUILD_DIR) -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(DEBUG_BUILD_DIR)

# Release build
release:
	cmake -S . -B $(RELEASE_BUILD_DIR) -G "Ninja" -DCMAKE_BUILD_TYPE=Release
	cmake --build $(RELEASE_BUILD_DIR)

# Legacy cmake target (uses release build)
cmake: release

# Test targets
test: debug
	cd $(DEBUG_BUILD_DIR) && ctest --output-on-failure

test-release: release
	cd $(RELEASE_BUILD_DIR) && ctest --output-on-failure

# Demo targets
demo: release
	cmake --build $(RELEASE_BUILD_DIR) --target demo
	$(RELEASE_BUILD_DIR)/examples/demo

training: release
	cmake --build $(RELEASE_BUILD_DIR) --target training
	$(RELEASE_BUILD_DIR)/examples/training

ingredient_folder: cmake
	cmake --build $(BUILD_DIR) --target ingredient_folder
	$(BUILD_DIR)/examples/ingredient_folder

examples: training demo

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all debug release cmake test test-release demo training examples clean

