// Port of the inline tests in skate-core/src/riding/grounded/state/corrections.rs.
#include "harness.hpp"
#include "s3/core/riding/grounded/state/corrections.hpp"

namespace {
using namespace s3::riding::grounded::state::corrections;
using s3::physics::force_queue::FORCE_CAPACITY;
using s3::rs::Result;
using s3::rs::Unit;

struct NudgeMath {
    using Error = Unit;  // Rust `()`

    std::array<float, 2> dots{};
    std::size_t dot_count = 0;
    std::size_t scale_count = 0;

    Result<float, Error> dot3(std::array<float, 4>, std::array<float, 4>) {
        const float value = dots[dot_count];
        dot_count += 1;
        return value;
    }
    Result<std::array<float, 4>, Error> scale_to_magnitude(std::array<float, 4> vector, float, float magnitude) {
        scale_count += 1;
        return s3::rs::map(vector, [magnitude](float lane) { return lane * magnitude; });
    }
};

PhysicsGroundState state() {
    PhysicsGroundState s{};
    s.anti_flip_torque_2624 = {1.0f, 0.0f, 0.0f, 0.0f};
    s.steering_push_scalar_2640 = 1.0f;
    s.straighten_scale_2672 = 1.0f;
    s.anti_flip_nudge_frames_2752 = 13;
    return s;
}

S3_TEST(nudge_unordered_comparisons_take_the_native_rejection_paths) {
    auto s = state();
    BoardForceQueue queue{};
    NudgeMath math{{s3::rs::f32::NAN_, 1.0f}, 0, 0};
    auto result =
        update_anti_flip_nudge(s, AntiFlipNudgeInput{0.0f, {1.0f, 0.0f, 0.0f, 0.0f}}, queue, math).unwrap();
    CHECK_EQ(s.anti_flip_nudge_frames_2752, 0);
    CHECK(!result.attempted);

    s.anti_flip_nudge_frames_2752 = 13;
    math.dots = {1.0f, s3::rs::f32::NAN_};
    math.dot_count = 0;
    result = update_anti_flip_nudge(s, AntiFlipNudgeInput{0.0f, {1.0f, 0.0f, 0.0f, 0.0f}}, queue, math).unwrap();
    CHECK(!result.attempted);
    CHECK_EQ(math.scale_count, std::size_t(0));
}

S3_TEST(nudge_latches_presence_even_when_the_native_queue_is_full) {
    auto s = state();
    BoardForceQueue queue{};
    for (std::size_t n = 0; n < FORCE_CAPACITY; ++n) {
        CHECK(queue.append(QueuedPointForce{}));
    }
    NudgeMath math{{1.0f, 1.0f}, 0, 0};

    const auto result =
        update_anti_flip_nudge(s, AntiFlipNudgeInput{0.0f, {1.0f, 0.0f, 0.0f, 0.0f}}, queue, math).unwrap();

    CHECK(result.attempted);
    CHECK(!result.queued);
    CHECK(s.anti_flip_nudge_applied_2723);
    CHECK_EQ(queue.entries().size(), FORCE_CAPACITY);
}

}  // namespace
