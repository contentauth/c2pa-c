OS := $(shell uname)
CFLAGS = -I. -Wall 
ifeq ($(OS), Darwin)
CFLAGS += -framework Security
endif
ifeq ($(OS), Linux)
CFLAGS = -pthread -Wl,--no-as-needed -ldl -lm
endif

release: 
	cargo build --release
	cbindgen --config cbindgen.toml --crate c2pa-c --output include/c2pa.h --lang c

test-c: release
	$(CC) $(CFLAGS) tests/main.c -o target/ctest -lc2pa_c -L./target/release 
	target/ctest

test-cpp: release
	$(CC) $(CFLAGS) -lstdc++ tests/test.cpp -o target/cpptest -lc2pa_c -L./target/release 
	target/cpptest

test: test-c test-cpp