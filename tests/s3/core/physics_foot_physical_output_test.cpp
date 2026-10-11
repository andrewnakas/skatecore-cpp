// Port of the inline #[cfg(test)] module in skate-core/src/physics/foot_physical_output.rs.
#include "harness.hpp"
#include "s3/core/physics/foot_physical_output.hpp"

namespace {
using namespace s3::physics::foot_physical_output;

S3_TEST(moving_board_frame_separates_world_rates_from_relative_foot_motion) {
    SkeletonPhysicalRecord record = SkeletonPhysicalRecord::default_();
    record.pose[0][3] = {10.0f, 2.0f, 3.0f, 0.0f};
    record.pose[15][3] = {10.2f, 2.0f, 3.3f, 0.0f};
    record.pose[19][3] = {9.8f, 2.0f, 2.7f, 0.0f};
    record.velocities[15] = {4.0f, 0.0f, 0.0f, 0.0f};
    record.velocities[19] = {5.0f, 0.0f, 0.0f, 0.0f};
    const FootPhysicalSettings settings{0.3f, 0.5f, {0.0f, 0.1f, 0.0f, 0.0f}};
    FootPhysicalState state;
    const auto initial = state.update(record, 0.25f, settings);
    CHECK_EQ(initial.within_deck_box, (std::array<bool, 2>{true, true}));
    CHECK_EQ(initial.world_velocity, (std::array<std::array<float, 4>, 2>{record.velocities[15], record.velocities[19]}));
    for (std::size_t part : {0, 15, 19}) {
        record.pose[part][3][0] += 1.0f;
    }
    const auto moved = state.update(record, 0.25f, settings);
    for (const auto& foot : moved.local_velocity)
        for (float value : foot) CHECK(s3::rs::abs(value) < 0.00001f);
    SkeletonPhysicalRecord boundaries = SkeletonPhysicalRecord::default_();
    boundaries.pose[15][3] = {0.3f, 0.0f, 0.0f, 0.0f};
    boundaries.pose[19][3] = {0.0f, 0.1f, 0.0f, 0.0f};
    CHECK_EQ(state.update(boundaries, 0.25f, settings).within_deck_box, (std::array<bool, 2>{false, false}));
}
}  // namespace
