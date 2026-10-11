// Port of skate-core/src/riding/tests/ground_orientation.rs.
#include "harness.hpp"
#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/riding/ground_orientation.hpp"

namespace {
using namespace s3::riding::ground_orientation;
using s3::physics::board_motion_output::length;

std::array<float, 4> lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

PointGraph<8> constant(float value) {
    return PointGraph<8>{
        {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f},
        {value, value, value, value, value, value, value, value},
    };
}
GroundOrientationSettings settings() {
    return GroundOrientationSettings{
        {1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        constant(1.0f),
        constant(0.0f),
        constant(0.0f),
        constant(0.0f),
        constant(1.0f),
        0.1f,
        2.0f,
        0.5f,
        0.6f,
        0,
    };
}
GroundOrientationInput input(Vector3 normal) {
    return GroundOrientationInput{
        UP,
        normal,
        normal,
        0.0f,
        4,
        0.0f,
        0.0f,
        UP,
        Vector3{0.0f, 0.0f, 1.0f},
        Vector3{0.0f, 0.0f, 1.0f},
        Vector3{1.0f, 0.0f, 0.0f},
        false,
    };
}

S3_TEST(stationary_filter_startup_has_no_false_tilt_or_acceleration) {
    const auto s = settings();
    auto state = GroundOrientation::new_(s);
    for (int i = 0; i < 120; ++i) {
        state.update(s, input(UP));
    }
    CHECK_EQ(state.up, UP);
    CHECK_EQ(state.ground_normal, UP);
    CHECK_EQ(state.up_velocity, Vector3::ZERO);
}

S3_TEST(changed_contact_normal_does_not_instantly_replace_the_skater_up) {
    const auto s = settings();
    auto state = GroundOrientation::new_(s);
    const Vector3 wall{1.0f, 0.0f, 0.0f};
    state.update(s, input(wall));
    CHECK_EQ(state.ground_normal, wall);
    CHECK_NE(state.up, state.ground_normal);
    CHECK(state.up.y > 0.99f && state.up.x > 0.0f && state.up.x < 0.04f);
    const auto& words = state.slow_filter.words();
    CHECK_EQ((std::array<std::uint32_t, 4>{words[4], words[5], words[6], words[7]}),
             s3::rs::to_bits(lanes(state.up)));
}

/// The landing NaN, at the point where it is made.
S3_TEST(an_up_velocity_decayed_to_a_denormal_square_does_not_turn_the_up_vector_nan) {
    const Vector3 settled{-3.2526066e-20f, 0.0f, 0.0f};
    CHECK_EQ(length(settled), 0.0f);
    const Vector3 clamped = clamp_length(settled, 2.0f * s3::rs::from_bits(0x3C888889u));
    CHECK_EQ(clamped, settled);

    // And end to end: the same decay driven through `update` from a settled stance.
    const auto s = settings();
    auto state = GroundOrientation::new_(s);
    state.up_velocity = settled;
    for (int i = 0; i < 8; ++i) {
        state.update(s, input(UP));
        for (float lane : {state.up.x, state.up.y, state.up.z}) {
            CHECK_MSG(s3::rs::is_finite(lane), "up went non-finite: " << state.up);
        }
    }
}

/// No finite stance the engine can hand in may produce a non-finite up vector.
S3_TEST(no_finite_input_sequence_turns_the_up_vector_non_finite) {
    const auto s = settings();
    std::uint32_t seed = 0x12345678u;
    auto rng = [&seed]() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return seed;
    };
    // A unit row, a zero row or an axis -- what the callers in `riding_outputs` actually pass.
    auto direction = [](std::uint32_t a, std::uint32_t b) -> Vector3 {
        switch (a % 6) {
            case 0:
                return Vector3::ZERO;
            case 1:
                return UP;
            case 2:
                return Vector3{0.0f, -1.0f, 0.0f};
            case 3:
                return Vector3{1.0f, 0.0f, 0.0f};
            case 4:
                return Vector3{0.0f, 0.0f, 1.0f};
            default: {
                const float yaw = static_cast<float>(b % 6283) / 1000.0f;
                const float pitch = static_cast<float>((b >> 13) % 6283) / 1000.0f;
                return Vector3{s3::rs::cos(yaw) * s3::rs::sin(pitch), s3::rs::cos(pitch),
                               s3::rs::sin(yaw) * s3::rs::sin(pitch)};
            }
        }
    };
    auto speed = [](std::uint32_t r) { return static_cast<float>((r >> 2) % 40000) / 1000.0f; };
    // Rust evaluates `direction(rng(), rng())` arguments left to right; sequence them explicitly.
    auto next_direction = [&]() {
        const std::uint32_t a = rng();
        const std::uint32_t b = rng();
        return direction(a, b);
    };
    auto non_finite = [](Vector3 v) {
        return !(s3::rs::is_finite(v.x) && s3::rs::is_finite(v.y) && s3::rs::is_finite(v.z));
    };
    for (std::uint32_t c = 0; c < 50000; ++c) {
        auto state = GroundOrientation::new_(s);
        for (int step = 0; step < 6; ++step) {
            // Braced initialisers are evaluated left to right in C++, matching the Rust literal.
            const GroundOrientationInput inputs{
                next_direction(),
                next_direction(),
                next_direction(),
                speed(rng()),
                static_cast<std::int32_t>(rng() % 5),
                rng() % 2 == 0 ? 0.0f : 1.0f,
                speed(rng()),
                next_direction(),
                next_direction(),
                next_direction(),
                next_direction(),
                rng() % 2 == 0,
            };
            const auto before = state;
            state.update(s, inputs);
            CHECK_MSG(!non_finite(state.up) && !non_finite(state.up_velocity),
                      "case " << c << " step " << step << ": up=" << state.up << " up_velocity="
                              << state.up_velocity << " entered with up=" << before.up
                              << " up_velocity=" << before.up_velocity);
        }
    }
}
}  // namespace
