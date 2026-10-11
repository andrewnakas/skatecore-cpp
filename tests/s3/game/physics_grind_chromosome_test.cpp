// Port of skate-game/src/physics/grind_chromosome/tests.rs (mounted as grind_chromosome::tests).
#include <cstring>

#include "harness.hpp"
#include "s3/game/physics/grind_chromosome.hpp"

using namespace s3::game::physics::grind_chromosome;

namespace {
Input input() {
    Input i{};
    i.category = 400;
    i.grinding_316 = true;
    i.air_event_439 = false;
    i.family = Family::FiftyFifty;
    i.basic_right_0 = {1.0f, 0.0f, 0.0f, 0.0f};
    i.basic_forward_32 = {0.0f, 0.0f, 1.0f, 0.0f};
    i.basic_position_48 = {0.0f, 0.0f, 0.0f, 0.0f};
    i.basic_location_axis_96 = {0.0f, 0.0f, 1.0f, 0.0f};
    i.basic_twist_axis_128 = {1.0f, 0.0f, 0.0f, 0.0f};
    i.basic_location_position_144 = {0.0f, 0.0f, 0.0f, 0.0f};
    i.feet_256_272 = {{{1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}}};
    i.fakie_155 = false;
    i.point = {0.0f, 0.0f, 0.0f, 0.0f};
    i.direction = {1.0f, 0.0f, 0.0f, 0.0f};
    i.normal = {0.0f, 1.0f, 0.0f, 0.0f};
    i.across = {0.0f, 0.0f, 1.0f, 0.0f};
    return i;
}
}  // namespace

S3_TEST(family_switch_does_not_fake_new_category_or_skip_scoring_delay) {
    auto state = Chromosome::new_(false);  // Explicit synthetic constructor observation.
    auto i = input();
    const auto first = state.update(i).value();
    CHECK(first.animation == std::optional<Components>(first.volatile_));
    CHECK(first.scoring == std::optional<Components>(first.volatile_));
    i.family = Family::FiveO;
    const auto switched = state.update(i).value();
    CHECK(switched.animation == first.animation);
    CHECK(switched.scoring == first.scoring);
    const auto repeated = state.update(i).value();
    CHECK(repeated.animation == std::optional<Components>(repeated.volatile_));
    for (int k = 0; k < 11; ++k) CHECK(state.update(i).value().scoring == first.scoring);
    CHECK(state.update(i).value().scoring == std::optional<Components>(repeated.volatile_));
}

S3_TEST(air_snapshot_clears_history_and_short_ground_history_does_not_replace_saved) {
    auto state = Chromosome::new_(false);
    auto i = input();
    i.category = 100;
    i.grinding_316 = false;
    for (int tick = 0; tick < 30; ++tick) {
        i.basic_position_48[0] = static_cast<float>(tick);
        state.update(i);
    }
    CHECK_EQ(state.reference().position[0], 0.0f);
    i.basic_position_48[0] = 30.0f;
    state.update(i);
    CHECK_EQ(state.reference().position[0], 1.0f);
    i.category = 200;
    i.air_event_439 = true;
    i.basic_position_48[0] = 99.0f;
    state.update(i);
    CHECK_EQ(state.reference().position[0], 99.0f);
    i.category = 100;
    i.air_event_439 = false;
    i.basic_position_48[0] = 1000.0f;
    state.update(i);
    CHECK_EQ(state.reference().position[0], 99.0f);
}

S3_TEST(twist_uses_128_not_forward32_and_names_keep_both_id_domains) {
    CHECK(pose::straight(input()));
    const auto name = names::lookup({0, 0, 0, 0, 0, 5}).value();
    CHECK(std::strcmp(name.attribute, "FS_DARKSLIDE") == 0);
    CHECK_EQ(name.skating_id, 17);
    CHECK_EQ(name.scorable_id, 270);
    CHECK(!names::lookup({0, 0, 0, 0, 4, 0}).has_value());
    CHECK(!names::lookup({0, 0, 0, 0, 0, UINT32_MAX}).has_value());
}
