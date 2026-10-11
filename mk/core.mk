# Stage C: skate-core math/physics port (src/s3/core) and its ported Rust #[test]s.
S3_CORE_CXX ?= c++
S3_CORE_CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Isrc -Itests/s3/core
S3_CORE_SRC := $(shell find src/s3/core -name '*.cpp' 2>/dev/null | sort)
S3_CORE_HDR := $(shell find src/s3/core -name '*.hpp' 2>/dev/null)
S3_CORE_TEST_SRC := $(sort $(wildcard tests/s3/core/*.cpp))
S3_CORE_OBJ := $(patsubst %.cpp,build/s3_core_obj/%.o,$(S3_CORE_SRC) $(S3_CORE_TEST_SRC))

build/s3_core_obj/%.o: %.cpp $(S3_CORE_HDR) tests/s3/core/harness.hpp
	@mkdir -p $(dir $@)
	$(S3_CORE_CXX) $(S3_CORE_CXXFLAGS) -c $< -o $@

build/s3_core_tests: $(S3_CORE_OBJ)
	$(S3_CORE_CXX) $(S3_CORE_CXXFLAGS) $^ -o $@

s3-core-test: build/s3_core_tests
	./build/s3_core_tests

.PHONY: s3-core-test
S3_TESTS += s3-core-test
