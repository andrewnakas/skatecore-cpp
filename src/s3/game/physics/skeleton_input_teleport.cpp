// Port of skate-game/src/physics/skeleton_input_teleport.rs.
#include "s3/game/physics/skeleton_input_teleport.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/animation_input.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::skeleton_input_runtime {

using sc::physics::board::BodyId;
using sc::physics::skeleton_animation_record::IDENTITY;

void SkeletonInputRuntime::reset_for_teleport(SkeletonOwners& owners,
                                              sc::physics::skeleton_body::SkeletonCollisionMode& mode,
                                              sc::physics::skeleton_body::SkeletonCollisionFeedback& feedback,
                                              sc::physics::skeleton_output::wobble::Wobble& wobble,
                                              bool& ground_elapsed_16505) {
    owners.animated.motion.reset_board_orientation_history();
    reenable_requested = true;
    invalid_target_reset = false;
    ground_elapsed_16505 = true;
    mode.reset_body_state(feedback);
    force_mode = 0;
    head_tracking_history = {};
    head_tracking_active = false;
    owners.ik.state = sc::animation::foot_ik::state::FootIkState{};
    //Native reset preserves the previously selected cached curves.
    wobble.time = 0.0f;
    wobble.amplitude = 0.0f;
    wobble.direction = 1.0f;
    wobble.active = false;
    wobble.landing = false;
    owners.animated.board_frames.animation_target = IDENTITY;
    owners.animated.board_offset = sc::physics::skeleton_board_offset::SkateboardOffset{};
    owners.animated.board_frames.lift_height = 0.0f;
}

rs::Result<Transform, std::string> SkeletonInputRuntime::update_teleport(
    const sc::physics::board_runtime::BoardRuntime& board, const Transform& reckoning_frame_816,
    ProcessedPhysicsInput& p, SkeletonOwners& owners, sc::animation::Slice<const NativeMatrix> globals,
    const CollisionInput& collision, sc::physics::rigid_body::RetailSimulationStep simulation) {
    teleporting = true;
    const auto part = board.part_transforms()[sc::physics::board::index(BodyId::Deck)];
    Transform deck{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t i = 0; i < 3; ++i) deck[axis][i] = part.basis.columns[axis][i];
    }
    deck[3] = {part.translation.x, part.translation.y, part.translation.z, 0.0f};
    auto& skeleton = owners.animated;
    skeleton.roots.update_teleport(deck, skeleton.animation_board, reckoning_frame_816);
    const Transform frame =
        skeleton.board_frames.prepare_teleport(skeleton.roots, drive_frames[0], deck, p.flags_2468);
    auto result = general_update(p, owners, globals, collision, simulation);
    //Keep the actual lifecycle flag coherent even if host data validation fails.
    teleporting = false;
    if (result.is_err()) return rs::make_err(result.unwrap_err());
    owners.animated.finish_ground();
    owners.animation_input.fields.flags2468 = p.flags_2468;
    return frame;
}

}  // namespace s3::game::physics::skeleton_input_runtime
