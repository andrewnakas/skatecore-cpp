// Port of skate-core/src/physics/tests/native_arithmetic.rs.
#include "harness.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace {
using namespace s3::physics::native_arithmetic;
namespace f32 = s3::rs::f32;

S3_TEST(host_math_normalizes_a_direction_without_using_its_homogeneous_lane) {
    const std::array<float, 4> direction{3.0f, 4.0f, 0.0f, 100.0f};
    const float inverse_length = reciprocal_square_root_estimate(dot3(direction, direction));
    const auto unit = s3::rs::map(direction, [&](float v) { return v * inverse_length; });
    CHECK(s3::rs::abs(dot3(unit, unit) - 1.0f) < 1.0e-6f);
    CHECK_EQ(dot3(direction, {4.0f, -3.0f, 0.0f, 50.0f}), 0.0f);
    CHECK_EQ(dot4(direction, {0.0f, 0.0f, 0.0f, 1.0f}), 100.0f);
}

S3_TEST(host_math_handles_scalar_geometry_and_ordinary_float_boundaries) {
    CHECK_EQ(reciprocal_estimate(4.0f), 0.25f);
    CHECK_EQ(reciprocal_square_root_estimate(16.0f), 0.25f);
    CHECK_EQ(reciprocal_estimate(-0.0f), f32::NEG_INFINITY);
    CHECK_EQ(reciprocal_square_root_estimate(f32::INFINITY_), 0.0f);
    CHECK(s3::rs::is_nan(reciprocal_square_root_estimate(-1.0f)));
    CHECK_EQ(vector_min(-2.0f, 3.0f), -2.0f);
    CHECK_EQ(vector_max(-2.0f, 3.0f), 3.0f);
}
}  // namespace
