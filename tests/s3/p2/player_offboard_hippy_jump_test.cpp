// Port of the inline tests in skate-core/src/player/offboard/hippy_jump.rs.
#include "harness.hpp"
#include "s3/core/player/offboard/hippy_jump.hpp"

namespace {
using namespace s3::player::offboard::hippy_jump;

S3_TEST(preserves_planar_velocity_and_replaces_up_component) {
    const auto output = calculate(Input{1.5f, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.5f, 0.0f, 0.0f},
                                        {0.0f, 1.0f, 0.0f, 0.0f}, {3.0f, -2.0f, 4.0f, 0.0f}});
    CHECK_EQ(output[0], 3.0f);
    CHECK_EQ(output[2], 4.0f);
    CHECK(std::fabs(output[1] - std::sqrt(19.6f)) < 1.0e-6f);
}
}  // namespace
