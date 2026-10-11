// Port of the inline `revert_guard_tests` module in skate-core/src/physics/skeleton_air_frames.rs.
#include "harness.hpp"
#include "s3/core/physics/skeleton_air_frames.hpp"

namespace {
using namespace s3::physics::skeleton_air_frames;
using s3::physics::skeleton_animation_record::IDENTITY;

bool finite(const Transform& m) {
    for (const auto& row : m)
        for (float v : row)
            if (!s3::rs::is_finite(v)) return false;
    return true;
}

/// The animation graph raises the dismount-revert request bit while this
/// engine's `air_dismount_revert_frames` is still always 0, which used to
/// divide pi by zero and poison every row of the skeleton root with NaN --
/// the "Non-finite torque_acceleration before shared solve" landing crash.
S3_TEST(zero_frame_dismount_revert_cannot_poison_the_air_root) {
    for (std::uint32_t frames : {0u, 1u, 12u}) {
        SkeletonRootFrames roots = SkeletonRootFrames::default_();
        roots.initialize_heading = false;
        update_known_air_roots(roots, IDENTITY, {1.0f, 2.0f, 3.0f, 1.0f}, {0.0f, 0.5f, 0.0f, 1.0f},
                               AirDismountRevert{true, frames, false});
        CHECK_MSG(finite(roots.animation_to_world),
                  "frames=" << frames << " left a non-finite animation_to_world");
        CHECK_MSG(finite(roots.heading_alignment) && finite(roots.world_to_animation),
                  "frames=" << frames << " left a non-finite derived frame");
    }
}
}  // namespace
