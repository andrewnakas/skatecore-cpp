# Stage A: skate-core animation/graph + skate-game graph_runtime/graph_host/skater_animation/
# animation_pose port and its ported Rust #[test]s. Own object dir, -MMD header tracking.
S3_ANIM_CXX ?= c++
S3_ANIM_CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Isrc -Itests/s3/core
# skate-core directories linked in (stage A's own plus the ones it depends on).
S3_ANIM_CORE_DIRS ?= src/s3/core/animation src/s3/core/graph src/s3/core/physics \
	$(wildcard src/s3/core/input src/s3/core/riding src/s3/core/player src/s3/core/air)
S3_ANIM_GAME_DIRS := $(wildcard src/s3/game/graph_runtime* src/s3/game/graph_host* \
	src/s3/game/skater_animation* src/s3/game/animation_pose* src/s3/game/animation*)
S3_ANIM_SRC := $(sort $(wildcard src/s3/core/*.cpp) \
	$(shell find $(S3_ANIM_CORE_DIRS) -name '*.cpp' 2>/dev/null) \
	$(wildcard $(S3_ANIM_GAME_DIRS:%=%.cpp)) \
	$(shell find $(filter-out %.cpp %.hpp,$(S3_ANIM_GAME_DIRS)) -name '*.cpp' 2>/dev/null) \
	$(wildcard src/s3/data/*.cpp))
S3_ANIM_TEST_SRC := $(sort $(wildcard tests/s3/anim/*.cpp))
S3_ANIM_OBJ := $(patsubst %.cpp,build/s3_anim_obj/%.o,$(S3_ANIM_SRC) $(S3_ANIM_TEST_SRC))

build/s3_anim_obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(S3_ANIM_CXX) $(S3_ANIM_CXXFLAGS) -MMD -MP -c $< -o $@

-include $(S3_ANIM_OBJ:.o=.d)

build/s3_anim_tests: $(S3_ANIM_OBJ)
	$(S3_ANIM_CXX) $(S3_ANIM_CXXFLAGS) $^ -o $@

s3-anim-test: build/s3_anim_tests
	./build/s3_anim_tests

.PHONY: s3-anim-test
S3_TESTS += s3-anim-test
