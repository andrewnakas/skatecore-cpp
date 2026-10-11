// Port of the #[cfg(test)] modules (tests, degenerate_tests) in
// skate-core/src/riding/reckoning_frames.rs.
#include "harness.hpp"
#include "s3/core/riding/reckoning_frames.hpp"

namespace {
using namespace s3::riding::reckoning_frames;
using s3::physics::skeleton_animation_record::compose_affine;
using V4 = std::array<float, 4>;

S3_TEST(body_flip_changes_system_but_retains_ground_and_preflip_frames) {
    auto state = ReckoningFrames::new_();
    state.heading = {0.0f, 0.0f, 1.0f, 0.0f};
    state.body_flip = AnimationPartTransform{{
        {0.0f, 0.0f, -1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {3.0f, 0.0f, 0.0f, 0.0f},
    }};
    state.calculate_transform({0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f});
    CHECK_EQ(state.unflipped, IDENTITY);
    CHECK_EQ(state.ground, IDENTITY);
    CHECK_EQ(state.system, state.body_flip);
    CHECK_EQ(compose_affine(state.system, state.inverse_system), IDENTITY);
    state.calculate_dynamic_lean({0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f});
    CHECK(s3::rs::abs(state.target_lean_angle) < 0.001f);
    state.lateral_tilt = {0.3f, 0.0f, 0.0f, 0.0f};
    const PointGraph<8> curve{};
    state.calculate_tilt(false, curve, curve);
    CHECK_EQ(state.lateral_tilt, (V4{0.3f, 0.0f, 0.0f, 0.0f}));
}

/// A vertical board makes `up` parallel to `heading`, which collapses the
/// cross products to zero.
S3_TEST(a_vertical_board_cannot_poison_the_persistent_heading) {
    auto frames = ReckoningFrames::new_();
    frames.heading = {0.0f, 1.0f, 0.0f, 0.0f};
    const V4 up{0.0f, 1.0f, 0.0f, 0.0f};
    const V4 before = frames.heading;
    frames.calculate_transform(up, {0.0f, 1.0f, 0.0f, 0.0f});
    auto all_finite = [](const auto& rows) {
        for (const auto& row : rows)
            for (float v : row)
                if (!s3::rs::is_finite(v)) return false;
        return true;
    };
    CHECK(all_finite(std::array<V4, 1>{frames.heading}));
    CHECK_EQ(frames.heading, before);
    CHECK(all_finite(frames.system) && all_finite(frames.ground));
    // And the frame still works normally afterwards.
    frames.calculate_transform({0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f});
    CHECK(all_finite(std::array<V4, 1>{frames.heading}));
}

/// An ordinary frame must be bit-identical to the unguarded leaf.
S3_TEST(a_healthy_frame_is_unchanged_by_the_guard) {
    const V4 v{0.3f, 0.0f, 0.9f, 0.0f};
    CHECK_EQ(normalize_or(v, {9.0f, 9.0f, 9.0f, 9.0f}), normalize(v));
}
}  // namespace
