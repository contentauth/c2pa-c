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

demo: debug
	cmake --build $(DEBUG_BUILD_DIR) --target demo
	$(DEBUG_BUILD_DIR)/examples/demo

demo-release: release
	cmake --build $(RELEASE_BUILD_DIR) --target demo
	$(RELEASE_BUILD_DIR)/examples/demo

training: debug
	cmake --build $(DEBUG_BUILD_DIR) --target training
	$(DEBUG_BUILD_DIR)/examples/training

training-release: release
	cmake --build $(RELEASE_BUILD_DIR) --target training
	$(RELEASE_BUILD_DIR)/examples/training

examples: training demo

clean:
	rm -rf $(BUILD_DIR)

clean-debug:
	rm -rf $(DEBUG_BUILD_DIR)

clean-release:
	rm -rf $(RELEASE_BUILD_DIR)

.PHONY: all debug release cmake test test-release demo training examples clean
