// Port of skate-core/src/physics/tests/filtered_state.rs.
#include "harness.hpp"
#include "s3/core/physics/filtered_state.hpp"

namespace {
using namespace s3::physics::filtered_state;

FilteredStateInput input(std::int32_t state, std::int32_t category) {
    FilteredStateInput in;
    in.physics_state = state;
    in.physics_category = category;
    in.anything_in_contact = false;
    in.physics_surface_type = 0;
    in.wall_ride_exit = false;
    in.targeting_grind = false;
    in.offboard_has_landed = false;
    in.offboard_on_deck = false;
    in.grind = GrindState::default_();
    in.last_grind_distance = 0.0f;
    return in;
}

S3_TEST(stairs_preserve_ground_then_contact_returns_air_to_ground) {
    FilteredState state = FilteredState::default_();
    FilteredStateInput stairs = input(100, 100);
    stairs.physics_surface_type = 8;
    CHECK_EQ(state.update(stairs).category, FilteredCategory::Ground);
    for (int i = 0; i < 9; ++i) {
        CHECK_EQ(state.update(input(200, 200)).category, FilteredCategory::Ground);
    }
    CHECK_EQ(state.update(input(200, 200)).category, FilteredCategory::Air);
    FilteredStateInput touching = input(200, 200);
    touching.anything_in_contact = true;
    const auto output = state.update(touching);
    CHECK_EQ(output.category, FilteredCategory::Ground);
    CHECK_EQ(output.previous_category, FilteredCategory::Air);
    state.update(input(702, 700));
    CHECK_EQ(state.update(input(200, 200)).category, FilteredCategory::Air);
}

S3_TEST(nonspecific_keeps_grind_metadata_and_leaving_clears_only_publication) {
    FilteredState state = FilteredState::default_();
    FilteredStateInput grinding = input(403, 400);
    grinding.grind = GrindState{
        3, 21, encode("fs smith"), encode("smith"), true, 0.25f, 0x123456789abcdef0ull,
        0xfedcba9876543210ull,
    };
    const auto entered = state.update(grinding);
    CHECK_EQ(entered.grind, grinding.grind);
    const auto retained = state.update(input(701, 700));
    CHECK(retained.grinding);
    CHECK_EQ(retained.grind, grinding.grind);
    const auto left = state.update(input(100, 100));
    CHECK(!left.grinding);
    CHECK_EQ(left.grind, GrindState::default_());
    CHECK_EQ(state.cached_grind, grinding.grind);
    state.reset();
    CHECK_EQ(state.cached_grind, GrindState::default_());
}
}  // namespace
