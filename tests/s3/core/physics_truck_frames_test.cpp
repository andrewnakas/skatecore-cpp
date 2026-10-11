// Port of skate-core/src/physics/tests/truck_frames.rs.
#include "harness.hpp"
#include "s3/core/physics/truck_frames.hpp"

namespace {
using namespace s3::physics::truck_frames;
using s3::physics::drive_frames::default_truck_transforms;

S3_TEST(each_target_changes_only_its_native_drive_and_keeps_anchor) {
    const auto base = default_truck_transforms();
    const auto rest = steering_drive_frames(base, {0.0f, 0.0f});
    const auto first = steering_drive_frames(base, {0.4f, 0.0f});
    CHECK_EQ(first[0], rest[0]);
    CHECK_NE(first[1].body_b.orientation, rest[1].body_b.orientation);
    CHECK_EQ(first[1].body_b.translation, rest[1].body_b.translation);
    const auto second = steering_drive_frames(base, {0.0f, 0.4f});
    CHECK_EQ(second[1], rest[1]);
    CHECK_NE(second[0].body_b.orientation, rest[0].body_b.orientation);
    CHECK_EQ(second[0].body_b.translation, rest[0].body_b.translation);
}
}  // namespace
