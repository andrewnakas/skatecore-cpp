# Stage P1: skate-core input/riding/air/scoring port and its ported Rust #[test]s.
# Own object dir with -MMD header tracking so header edits rebuild only dependants.
# Overridable for partial builds, e.g.
#   make s3-p1-test S3_P1_OBJDIR=build/p1_air S3_P1_MODULES=air S3_P1_TEST_GLOB='tests/s3/p1/air_*.cpp'
S3_P1_CXX ?= c++
S3_P1_CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Isrc -Itests/s3/core
S3_P1_OBJDIR ?= build/s3_p1_obj
S3_P1_MODULES ?= input riding air scoring
S3_P1_TEST_GLOB ?= tests/s3/p1/*.cpp
S3_P1_SRC := $(sort $(wildcard src/s3/core/*.cpp) \
	$(shell find $(addprefix src/s3/core/,$(S3_P1_MODULES) physics animation) -name '*.cpp' 2>/dev/null))
S3_P1_TEST_SRC := $(sort tests/s3/p1/main.cpp $(wildcard $(S3_P1_TEST_GLOB)))
S3_P1_OBJ := $(patsubst %.cpp,$(S3_P1_OBJDIR)/%.o,$(S3_P1_SRC) $(S3_P1_TEST_SRC))

$(S3_P1_OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(S3_P1_CXX) $(S3_P1_CXXFLAGS) -MMD -MP -c $< -o $@

-include $(S3_P1_OBJ:.o=.d)

$(S3_P1_OBJDIR)/s3_p1_tests: $(S3_P1_OBJ)
	@echo "link $@"
	@$(S3_P1_CXX) $(S3_P1_CXXFLAGS) $^ -o $@

s3-p1-test: $(S3_P1_OBJDIR)/s3_p1_tests
	./$(S3_P1_OBJDIR)/s3_p1_tests

.PHONY: s3-p1-test
S3_TESTS += s3-p1-test
