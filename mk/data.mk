# Stage D: s3::data loaders (SKPK1 VFS, JSON, VLT collections, ABIN banks, state graphs,
# joystick patterns, input config, .skate maps + collision, scoring) and their tests.
S3_DATA_CXX ?= c++
S3_DATA_CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall
S3_DATA_SRC := $(wildcard src/s3/data/*.cpp)
S3_DATA_HDR := $(wildcard src/s3/data/*.hpp)
S3_DATA_TESTS := $(wildcard tests/s3/data/*.cpp)

build/s3_data_tests: $(S3_DATA_SRC) $(S3_DATA_HDR) $(S3_DATA_TESTS) $(wildcard tests/s3/data/*.hpp)
	@mkdir -p build
	$(S3_DATA_CXX) $(S3_DATA_CXXFLAGS) -Isrc $(S3_DATA_SRC) $(S3_DATA_TESTS) -o $@

s3-data-test: build/s3_data_tests
	./build/s3_data_tests

S3_TESTS += s3-data-test
.PHONY: s3-data-test
