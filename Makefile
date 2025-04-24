OS := $(shell uname)
CFLAGS = -I. -Wall
ENV =
ifeq ($(findstring _NT, $(OS)), _NT)
#CFLAGS += -L./target/release -lc2pa_c
CC := gcc
CXX := g++
# ENV = PATH="$(shell pwd)/target/release:$(PATH)"
endif
ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif
ifeq ($(OS), Linux)
CFLAGS = -pthread -Wl,--no-as-needed -ldl -lm
# ENV = LD_LIBRARY_PATH=target/release
endif

show:
	@echo $(OS)
	@echo $(CFLAGS)
	@echo $(CC)

BUILD_DIR = build
FETCHED_INCLUDE_DIR = $(BUILD_DIR)/_deps/c2pa_prebuilt-src/include
CFLAGS += -I$(FETCHED_INCLUDE_DIR)
FETCHED_LIB_DIR = $(BUILD_DIR)/_deps/c2pa_prebuilt-src/lib
CFLAGS += -L$(FETCHED_LIB_DIR)
cmake:
	mkdir -p $(BUILD_DIR)
	cmake -S./ -B./$(BUILD_DIR) -G "Ninja"

test-c: cmake
	$(CC) $(CFLAGS) tests/test.c -o $(BUILD_DIR)/ctest -lc2pa_c
	$(ENV) $(BUILD_DIR)/ctest

test-cpp: cmake
	cd $(BUILD_DIR) && ninja
	mkdir -p $(BUILD_DIR)/src/tests/fixtures
	cp -r tests/fixtures/* $(BUILD_DIR)/src/tests/fixtures/
ifeq ($(findstring _NT, $(OS)), _NT)
	@echo "Current directory: $$(pwd)"
	@echo "Test exe location: $$(pwd)/$(BUILD_DIR)/src/c2pa_c_tests.exe"
	cp $(BUILD_DIR)/_deps/c2pa_prebuilt-src/lib $(BUILD_DIR)/src/
	cd $(BUILD_DIR)/src && $(ENV) cmd /c "set PATH=$$(pwd);%PATH% && c2pa_c_tests.exe"
else
	cd $(BUILD_DIR)/src && $(ENV) ./c2pa_c_tests
endif

demo: cmake 
	cmake --build ./$(BUILD_DIR) --target demo
	cd $(BUILD_DIR); examples/demo

training: cmake 
	cmake --build ./$(BUILD_DIR) --target training
	cd $(BUILD_DIR); examples/training

examples: training demo

test: test-c test-cpp

all: test examples
