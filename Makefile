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
