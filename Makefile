# Native build + tests for the skate library. The web build compiles src/*.cpp through engine/.
CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -g
SAN ?= $(if $(filter Darwin,$(shell uname)),-fsanitize=undefined,-fsanitize=address,undefined)
SRC := $(wildcard src/*.cpp)

build/skate_tests: $(SRC) tests/test_main.cpp src/*.hpp include/skate_api.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(SAN) -Iinclude $(SRC) tests/test_main.cpp -o $@

test: build/skate_tests
	./build/skate_tests

clean:
	rm -rf build

.PHONY: test clean

# Full-runtime port (src/s3): each stage adds its own fragment in mk/<stage>.mk with an
# `s3-<stage>-test` target; `make s3-test` runs them all.
S3_TESTS :=
-include mk/*.mk
s3-test: $(S3_TESTS)
.PHONY: s3-test
