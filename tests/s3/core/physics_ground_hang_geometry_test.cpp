// Port of the inline tests in skate-core/src/physics/ground_hang_geometry.rs.
#include "harness.hpp"
#include "s3/core/physics/ground_hang_geometry.hpp"

namespace {
using namespace s3::physics::ground_hang_geometry;
using H = std::array<std::optional<float>, 6>;

S3_TEST(hanging_geometry_distinguishes_rail_side_and_blocked_deck) {
    const HangGeometryInput input{Vector3::ZERO, Vector3{0.0f, 0.0f, 2.0f}, Vector3{4.0f, 5.0f, 1.0f}};
    const auto lines = s3::rs::unwrap(hang_lines(input, 0.243f));
    CHECK_EQ(lines[0].start, (Vector3{0.09f, 0.04f, 1.0f}));
    CHECK_EQ(lines[0].end, (Vector3{0.09f, -0.04f, 1.0f}));
    CHECK_EQ(lines[1].start.x, -0.09f);
    CHECK_EQ(lines[5].radius, 0.001f);
    CHECK_EQ(lines[5].start, (Vector3{0.09f, 0.09f, 1.0f}));
    CHECK_EQ(lines[5].end, (Vector3{-0.09f, 0.09f, 1.0f}));
    CHECK(hung_classification(H{0.5f, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt}));
    CHECK(!hung_classification(H{0.5f, std::nullopt, 0.64f, std::nullopt, std::nullopt, std::nullopt}));
    CHECK(!hung_classification(H{0.5f, 0.5f, std::nullopt, std::nullopt, std::nullopt, std::nullopt}));
    CHECK(!hung_classification(H{std::nullopt, std::nullopt, std::nullopt, std::nullopt, 0.5f, std::nullopt}));
}
}  // namespace
