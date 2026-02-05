OS := $(shell uname)

# Build directories
BUILD_DIR = build
DEBUG_BUILD_DIR = build/debug
RELEASE_BUILD_DIR = build/release

# CMake options (can be overridden via environment)
CMAKE_OPTS :=
ifdef C2PA_BUILD_FROM_SOURCE
	CMAKE_OPTS += -DC2PA_BUILD_FROM_SOURCE=$(C2PA_BUILD_FROM_SOURCE)
endif
ifdef C2PA_RS_PATH
	CMAKE_OPTS += -DC2PA_RS_PATH=$(C2PA_RS_PATH)
endif

# Default target
all: clean test examples

clean:
	rm -rf $(BUILD_DIR)

# Debug build
debug:
	cmake -S . -B $(DEBUG_BUILD_DIR) -G "Ninja" -DCMAKE_BUILD_TYPE=Debug $(CMAKE_OPTS)
	cmake --build $(DEBUG_BUILD_DIR)

# Release build
release:
	cmake -S . -B $(RELEASE_BUILD_DIR) -G "Ninja" -DCMAKE_BUILD_TYPE=Release $(CMAKE_OPTS)
	cmake --build $(RELEASE_BUILD_DIR)

# Legacy cmake target (uses release build)
cmake: release

# Test targets
test: clean debug
	cd $(DEBUG_BUILD_DIR) && ctest --output-on-failure

test-release: clean release
	cd $(RELEASE_BUILD_DIR) && ctest --output-on-failure

# Run only the C test (test.c)
test-c: clean release
	@echo "Running C test only..."
ifeq ($(OS),Darwin)
	DYLD_LIBRARY_PATH=$(RELEASE_BUILD_DIR)/tests:$$DYLD_LIBRARY_PATH ./$(RELEASE_BUILD_DIR)/tests/ctest
else
	LD_LIBRARY_PATH=$(RELEASE_BUILD_DIR)/tests:$$LD_LIBRARY_PATH ./$(RELEASE_BUILD_DIR)/tests/ctest
endif

# Run only the C++ tests
test-cpp: clean release
	@echo "Running C++ tests only..."
ifeq ($(OS),Darwin)
	DYLD_LIBRARY_PATH=$(RELEASE_BUILD_DIR)/tests:$$DYLD_LIBRARY_PATH ./$(RELEASE_BUILD_DIR)/tests/c2pa_c_tests
else
	LD_LIBRARY_PATH=$(RELEASE_BUILD_DIR)/tests:$$LD_LIBRARY_PATH ./$(RELEASE_BUILD_DIR)/tests/c2pa_c_tests
endif

# Demo targets
demo: release
	cmake --build $(RELEASE_BUILD_DIR) --target demo
	$(RELEASE_BUILD_DIR)/examples/demo

training: release
	cmake --build $(RELEASE_BUILD_DIR) --target training
	$(RELEASE_BUILD_DIR)/examples/training

examples: training demo

.PHONY: all debug release cmake test test-release demo training examples clean

# Build C API docs with Doxygen
docs:
	./scripts/generate_api_docs.sh
