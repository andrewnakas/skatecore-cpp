// Port of skate-core/src/player/offboard/landing_state/tests.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/landing_state.hpp"

namespace {
using namespace s3::player::offboard::landing_state;
using s3::player::offboard::landing_deck::GRAVITY;
using s3::player::offboard::landing_deck::STEP;

const Vector UP{0.0f, 1.0f, 0.0f, 0.0f};

Settings settings() { return Settings{5.0f, 0.1f, 0.2f, 0.01f, 0.02f, 10.0f}; }
Entry entry() {
    Entry e;
    e.previous_category = 100;
    e.hippy = true;
    e.strength = 0.5f;
    e.board_position = {0.0f, 0.0f, 0.0f, 0.0f};
    e.com_position = {0.0f, 1.0f, 0.0f, 0.0f};
    e.com_velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    e.board_velocity = {2.0f, 0.0f, 0.0f, 0.0f};
    e.up = UP;
    e.hips_up = {1.0f, 0.0f, 0.0f, 0.0f};
    e.animation_right = {1.0f, 0.0f, 0.0f, 0.0f};
    e.reversed = false;
    return e;
}

S3_TEST(hippy_initializes_the_shared_manager_and_follows_gravity) {
    State state;
    Manager manager;
    state.enter(manager, entry());
    CHECK(manager.trajectory_valid_164 && manager.force_257);
    CHECK_EQ(manager.trajectory_32.velocity[0], 2.0f);
    CHECK_EQ(manager.trajectory_32.acceleration, GRAVITY);
    const auto t = manager.trajectory_32;
    CHECK(t.position_at(0.1f)[1] > t.position[1]);
    CHECK(t.velocity_at(1.0f)[1] < 0.0f);
    CHECK(t.position_at(1.0f)[1] < t.position[1]);
    CHECK_EQ(state.takeoff_frames, 2);
}

S3_TEST(biped_entry_preserves_the_actual_manager) {
    State state;
    Manager manager;
    state.enter(manager, entry());
    Entry i = entry();
    const auto velocity = manager.trajectory_32.velocity;
    i.previous_category = 500;
    i.hippy = false;
    state.enter(manager, i);
    CHECK_EQ(manager.elapsed_160, STEP);
    CHECK_EQ(manager.trajectory_32.velocity, velocity);
}

S3_TEST(spin_input_is_not_applied_on_first_frame_or_when_descending) {
    State state;
    state.hippy = true;
    state.align(settings(), {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, 1.0f, 2.0f, 1.0f);
    CHECK_EQ(state.applied_spin, 0.0f);
    state.time_to_land = 1.0f;
    state.align(settings(), {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, 1.0f, 2.0f, 1.0f);
    CHECK_EQ(state.applied_spin, -0.01f);
    state.spin_rate = 0.0f;
    state.align(settings(), {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, 1.0f, -2.0f, 1.0f);
    CHECK_EQ(state.applied_spin, 0.0f);
}

S3_TEST(accurate_landing_uses_physical_feet_and_relative_board_velocity) {
    const float t = State::accurate_time(0.0f, 0.0f, -1.0f, {0.3f, 0.3f}, 4);
    CHECK(t > 0.0f && t < 0.2f);
    const float airborne = State::accurate_time(0.0f, 0.0f, -1.0f, {0.3f, 0.3f}, 0);
    CHECK(airborne > t);
    CHECK_EQ(State::accurate_time(0.0f, 0.0f, 0.0f, {-2.0f, -2.0f}, 4), 0.0f);
}
}  // namespace
