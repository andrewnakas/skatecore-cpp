# Stage P3: skate-game simulation (src/s3/game: physics/**, input/**, grind_world/**, skate_world,
# scoring_runtime, ...) + its ported Rust #[test]s, and the `s3trace` parity tool.
# Own object dir with -MMD header tracking. Partial builds, e.g. syntax-check one file:
#   make build/s3_game_obj/src/s3/game/physics/clock.o
S3_GAME_CXX ?= c++
S3_GAME_CXXFLAGS ?= -std=c++17 -O2 -ffp-contract=off -Wall -Wextra -Wdouble-promotion -Isrc -Itests/s3/core
S3_GAME_OBJDIR ?= build/s3_game_obj
S3_GAME_TEST_GLOB ?= tests/s3/game/*.cpp
S3_GAME_LIB_SRC := $(sort $(shell find src/s3/core src/s3/game -name '*.cpp' 2>/dev/null) \
	$(wildcard src/s3/data/*.cpp))
S3_GAME_TEST_SRC := $(sort tests/s3/game/main.cpp $(wildcard $(S3_GAME_TEST_GLOB)))
S3_GAME_LIB_OBJ := $(patsubst %.cpp,$(S3_GAME_OBJDIR)/%.o,$(S3_GAME_LIB_SRC))
S3_GAME_TEST_OBJ := $(patsubst %.cpp,$(S3_GAME_OBJDIR)/%.o,$(S3_GAME_TEST_SRC))

$(S3_GAME_OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(S3_GAME_CXX) $(S3_GAME_CXXFLAGS) -MMD -MP -c $< -o $@

-include $(S3_GAME_LIB_OBJ:.o=.d) $(S3_GAME_TEST_OBJ:.o=.d) $(S3_GAME_OBJDIR)/tools/s3trace.d

$(S3_GAME_OBJDIR)/libs3game.a: $(S3_GAME_LIB_OBJ)
	@rm -f $@
	@ar rcs $@ $^

$(S3_GAME_OBJDIR)/s3_game_tests: $(S3_GAME_TEST_OBJ) $(S3_GAME_OBJDIR)/libs3game.a
	@echo "link $@"
	@$(S3_GAME_CXX) $(S3_GAME_CXXFLAGS) $^ -o $@

build/s3trace: $(S3_GAME_OBJDIR)/tools/s3trace.o $(S3_GAME_OBJDIR)/libs3game.a
	@echo "link $@"
	@$(S3_GAME_CXX) $(S3_GAME_CXXFLAGS) $^ -o $@

s3-game-test: $(S3_GAME_OBJDIR)/s3_game_tests
	./$(S3_GAME_OBJDIR)/s3_game_tests

s3trace: build/s3trace

.PHONY: s3-game-test s3trace
S3_TESTS += s3-game-test

# Prebuild only the skate-core + skate-data objects the game library links against.
S3_GAME_DEP_OBJ := $(filter $(S3_GAME_OBJDIR)/src/s3/core/% $(S3_GAME_OBJDIR)/src/s3/data/%,$(S3_GAME_LIB_OBJ))
s3-game-deps: $(S3_GAME_DEP_OBJ)
.PHONY: s3-game-deps
