// Port of skate-game/src/physics/handplant/trajectory.rs (#[cfg(test)] mod tests)
#include "harness.hpp"
#include "s3/game/physics/handplant.hpp"

using namespace s3::game::physics::handplant;
using namespace s3::game::physics::handplant::trajectory;
namespace rs = s3::rs;

S3_TEST(stock_handplant_apex_is_above_coping_and_back_towards_ramp) {
    const V coping{-7.0f, 3.8850045f, 5.5987973f, 0.0f};
    const V apex = apex_position(coping, {0.0f, 0.0f, 1.0f, 0.0f}, 0.8f, -0.3f);
    CHECK(rs::abs(apex[1] - 4.649274f) < 0.00001f);
    CHECK(rs::abs(apex[2] - 5.362381f) < 0.00001f);
    //Observed late attempt COM previously exceeded the incorrectly lowered
    //apex. The original geometry admits a finite ascending trajectory.
    const V start{-6.9291053f, 3.925457f, 4.9562364f, 0.0f};
    const auto t = to_apex(start, apex);
    for (float v : t.velocity) CHECK(rs::is_finite(v));
    CHECK(t.duration > 0.0f);
    const V end = t.position_at(t.duration);
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(rs::abs(end[i] - apex[i]) < 0.00001f);
    }
    CHECK(rs::abs(t.velocity_at(t.duration)[1]) < 0.00001f);
}

S3_TEST(apex_angle_zero_places_com_directly_above_lip_for_either_edge_direction) {
    for (float direction : {-1.0f, 1.0f}) {
        const V apex = apex_position({2.0f, 3.0f, 4.0f, 0.0f}, {direction, 0.0f, 0.0f, 0.0f}, 0.8f, 0.0f);
        CHECK(rs::abs(apex[1] - 3.8f) < 0.00001f);
        CHECK(rs::abs(apex[0] - 2.0f) < 0.00001f);
        CHECK(rs::abs(apex[2] - 4.0f) < 0.00001f);
    }
}
