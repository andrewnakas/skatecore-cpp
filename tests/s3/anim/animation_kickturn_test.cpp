// Port of skate-core/src/animation/tests/kickturn.rs.
#include "harness.hpp"
#include "s3/core/animation/kickturn.hpp"

using namespace s3::animation::kickturn;

namespace {
PointGraph<8> graph(std::array<float, 8> y) {
    return PointGraph<8>{{0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 1.0f}, y};
}
Settings settings() {
    return Settings{graph({0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 1.0f}),
                    graph({2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f})};
}
}  // namespace

S3_TEST(first_update_uses_unscaled_height_seed_without_advancing_time) {
    const auto s = settings();
    auto state = State::new_();
    state.begin(s);
    state.capture_animation_length(1.0f);
    const Parameters p{0.3f, 0.5f};
    const Output first = state.update(0.25f, -0.8f, p, s);
    CHECK_EQ(first, (Output{-0.3f, -0.0f}));
    CHECK(!state.needs_animation_length());
    //Absent/zero input keeps the original negative intent direction.
    CHECK_EQ(state.update(0.25f, 0.0f, p, s).spin, -0.125f);
    CHECK_EQ(state.update(0.25f, -0.0f, p, s).spin, -0.25f);
    state.begin(s);
    state.capture_animation_length(1.0f);
    CHECK_EQ(s3::rs::to_bits(state.update(0.25f, 0.0f, p, s).spin), 0u);
}

S3_TEST(phase_clamp_preserves_zero_length_and_end_behavior) {
    const auto s = settings();
    const Parameters p{0.3f, 0.5f};
    auto state = State::new_();
    state.begin(s);
    state.capture_animation_length(0.0f);
    //0/0 follows the original fsel path to phase1, rather than phase0.
    CHECK_EQ(state.update(0.25f, 1.0f, p, s).spin, 0.5f);
    CHECK_EQ(state.update(10.0f, -1.0f, p, s).spin, -0.5f);
}
