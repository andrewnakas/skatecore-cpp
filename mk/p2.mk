# Stage P2: skate-core player/** port and its ported Rust #[test]s.
# Own object dir with -MMD header tracking. Overridable for partial builds, e.g.
#   make s3-p2-test S3_P2_BIN=build/s3_p2_obj/p2_wipeout \
#     S3_P2_PLAYER_SRC="$(find src/s3/core/player/wipeout* -name '*.cpp')" \
#     S3_P2_TEST_GLOB='tests/s3/p2/player_wipeout*.cpp'
S3_P2_CXX ?= c++
S3_P2_CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Isrc -Itests/s3/core
S3_P2_OBJDIR ?= build/s3_p2_obj
S3_P2_BIN ?= $(S3_P2_OBJDIR)/s3_p2_tests
S3_P2_PLAYER_SRC ?= $(shell find src/s3/core/player -name '*.cpp' 2>/dev/null)
S3_P2_TEST_GLOB ?= tests/s3/p2/*.cpp
# Non-player objects the player code links against (stage C physics + shared helpers;
# stage A/P1 sources are picked up when they exist).
# riding/air: only the stage-C files physics links against (stage P1 is rewriting those trees).
S3_P2_DEP_SRC := $(wildcard src/s3/core/*.cpp) \
	$(shell find src/s3/core/physics src/s3/core/animation/foot_ik src/s3/core/air/trajectory/grind_surface \
	  -name '*.cpp' 2>/dev/null) \
	$(wildcard src/s3/core/riding/braking.cpp src/s3/core/riding/collision_response.cpp \
	  src/s3/core/riding/ground_orientation.cpp src/s3/core/riding/push.cpp \
	  src/s3/core/riding/reckoning_frames.cpp src/s3/core/air/body_flip.cpp src/s3/core/air/body_spin.cpp \
	  src/s3/core/air/trajectory/grind_surface.cpp src/s3/core/input/angle.cpp \
	  src/s3/core/input/animation_packet.cpp src/s3/core/animation/output/actor_packet.cpp \
	  src/s3/core/air/trajectory.cpp src/s3/core/air/trajectory/prediction.cpp \
	  src/s3/core/air/trajectory/query.cpp src/s3/core/animation/foot_ik/external.cpp)
S3_P2_SRC := $(sort $(S3_P2_DEP_SRC) $(strip $(S3_P2_PLAYER_SRC)))
S3_P2_TEST_SRC := $(sort tests/s3/p2/main.cpp $(wildcard $(strip $(S3_P2_TEST_GLOB))))
S3_P2_OBJ := $(patsubst %.cpp,$(S3_P2_OBJDIR)/%.o,$(S3_P2_SRC) $(S3_P2_TEST_SRC))

$(S3_P2_OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(S3_P2_CXX) $(S3_P2_CXXFLAGS) -MMD -MP -c $< -o $@

-include $(S3_P2_OBJ:.o=.d)

$(S3_P2_BIN): $(S3_P2_OBJ)
	@echo "link $@"
	@$(S3_P2_CXX) $(S3_P2_CXXFLAGS) $^ -o $@

s3-p2-test: $(S3_P2_BIN)
	./$(S3_P2_BIN)

.PHONY: s3-p2-test
S3_TESTS += s3-p2-test
