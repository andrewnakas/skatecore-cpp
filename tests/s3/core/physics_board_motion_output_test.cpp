// Port of the inline `normalize_row_tests` in skate-core/src/physics/board_motion_output.rs.
#include "harness.hpp"
#include "s3/core/physics/board_motion_output.hpp"

namespace {
using namespace s3::physics::board_motion_output;
namespace f32 = s3::rs::f32;
namespace native_arithmetic = s3::physics::native_arithmetic;

/// The guard's contract: a row retail could produce normalises exactly as before, and a
/// degenerate or non-finite one keeps the fallback instead of becoming NaN.
S3_TEST(a_degenerate_or_non_finite_row_keeps_the_fallback) {
    const std::array<float, 4> fallback{1.0f, 0.0f, 0.0f, 0.0f};
    CHECK_EQ(normalize_row_or({0.0f, 0.0f, 0.0f, 0.0f}, fallback), fallback);
    CHECK_EQ(normalize_row_or({f32::NAN_, 0.0f, 0.0f, 0.0f}, fallback), fallback);
    CHECK_EQ(normalize_row_or({f32::INFINITY_, 0.0f, 0.0f, 0.0f}, fallback), fallback);
    // An ordinary row is untouched: same arithmetic as before the guard.
    const std::array<float, 4> row{0.0f, 3.0f, 4.0f, 0.0f};
    const float reciprocal = inverse_length_squared(native_arithmetic::dot3(row, row), 2);
    CHECK_EQ(normalize_row_or(row, fallback), s3::rs::map(row, [&](float v) { return v * reciprocal; }));
}
}  // namespace
