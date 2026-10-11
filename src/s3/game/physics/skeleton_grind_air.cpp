// Port of skate-game/src/physics/skeleton_grind_air.rs.
//!82BDCFB0 at the existing ProcessData boundary; never a physical teleport.
#include "s3/game/physics/skeleton_grind_air.hpp"

#include "s3/game/physics/animated_skeleton.hpp"

namespace s3::game::physics::skeleton_grind_air {

namespace grind_air = ::s3::physics::grind_air;
using ::s3::physics::skeleton_animation_record::map_animation_parts;

rs::Result<bool, std::string> update(grind_air::GrindAir& state, const grind_air::Settings& settings,
                                     ::s3::physics::skeleton_animation_record::AnimationPartTransform processed_deck,
                                     const ::s3::player::input_phase::ProcessedPhysicsInput& p,
                                     animated_skeleton::AnimatedSkeleton& animated,
                                     const std::vector<::s3::animation::output::NativeMatrix>& globals) {
    grind_air::Input input;
    input.active = true;
    input.board = processed_deck;
    input.velocity = rs::from_bits(p.vectors_400_416[0]);
    input.angular_velocity = rs::from_bits(p.vectors_720_784_800_816_832_864[0]);
    input.up = rs::from_bits(p.vectors_544_560_592_608[0]);
    input.timestep = p.timestep_2604;
    input.flags_2468 = p.flags_2468;
    input.flags_2472 = p.flags_2472;
    input.flags_2480 = p.flags_2480;
    input.flags_2484 = p.flags_2484;
    S3G_TRY(const auto adjustment, state.update(input, settings));
    if (!adjustment) {
        return false;
    }
    // ProcessData maps this frame before the native producer; do not use last pose.
    S3G_TRY(const auto parts, map_animation_parts(globals, animated.bone_indices, animated.physics_frames));
    grind_air::PoseInput pose;
    pose.animation_to_world = animated.roots.animation_to_world;
    pose.world_to_animation = animated.roots.world_to_animation;
    pose.physical_forward = animated.board_frames.physical_board[2];
    pose.animation_board_position = parts[0][3];
    const auto transform = adjustment->local_transform(pose);
    animated.board_offset.refresh_transform(transform);
    return true;
}

}  // namespace s3::game::physics::skeleton_grind_air
