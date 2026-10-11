// Port of skate-game/src/physics/tests/clock.rs (mounted as physics::clock::tests).
#include "harness.hpp"
#include "s3/core/camera/slow_motion.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/game/physics/clock.hpp"

using namespace s3::game::physics::clock;
using s3::camera::SimulationRateRequest;
using s3::camera::SlowMotionController;
using s3::camera::SlowMotionSettings;

S3_TEST(camera_rate_requests_expire_and_preserve_message_order) {
    SimulationClock clock{};
    const auto normal = clock.period();
    const SimulationRateRequest slow{1.0f / 35.4f, 1};
    CHECK(clock.apply(slow).is_ok());
    clock.finish_tick();
    CHECK(clock.period() == Duration::from_nanos(28571400));
    clock.finish_tick();
    CHECK(clock.period() == normal);

    // An End followed by a new Begin must retain the new request.
    CHECK(clock.apply(SlowMotionController::end()).is_ok());
    CHECK(clock.apply(slow).is_ok());
    clock.finish_tick();
    CHECK(clock.period() != normal);
    CHECK(clock.apply(SlowMotionController::end()).is_ok());
    clock.finish_tick();
    CHECK(clock.period() == normal);

    SimulationRateRequest zero = slow;
    zero.ticks = 0;
    CHECK(clock.apply(zero).is_ok());
    for (int i = 0; i < 179; ++i) clock.finish_tick();
    CHECK(clock.period() != normal);
    clock.finish_tick();
    CHECK(clock.period() == normal);
}

S3_TEST(camera_slow_motion_waits_for_a_positive_air_prediction) {
    SlowMotionSettings settings{};
    for (std::size_t i = 0; i < 16; ++i) {
        settings.timescale.x[i] = float(i) / 15.0f;
        settings.timescale.y[i] = float(i) / 15.0f;
    }
    settings.fps_at_scale_one = 35.0f;
    auto [controller, request] = SlowMotionController::begin(settings);
    (void)request;
    const auto waiting = controller.update(0.1f, s3::rs::f32::NAN_, settings);
    CHECK(s3::rs::abs(waiting.timestep - 1.0f / 57.5f) < 1e-6f);
    const auto predicted = controller.update(0.1f, 1.0f, settings);
    const float expected = 1.0f / (60.0f - 25.0f * (0.2f / 1.1f));
    CHECK(s3::rs::abs(predicted.timestep - expected) < 1e-6f);
}
