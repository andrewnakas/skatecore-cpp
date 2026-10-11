// Port of skate-game/src/physics/offboard/pose_adjust.rs
#include "s3/game/physics/offboard/pose_adjust.hpp"

#include "s3/game/physics/animated_skeleton.hpp"

namespace s3::game::physics::offboard::pose_adjust {

namespace native = ::s3::player::offboard::pose_adjust;

Status update(animated_skeleton::AnimatedSkeleton& animated,
              sc::physics::Slice<const sc::physics::skeleton_animation_record::AnimationPartTransform> globals,
              std::array<std::size_t, 2> reparented_hands,
              const sc::player::input_phase::ProcessedPhysicsInput& processed) {
    if (processed.category_2512 != 500 || processed.state_2508 == 503) return ok();
    const std::size_t hand = native::selected_hand(processed.flags_2476);
    const std::array<std::size_t, 2> parts{3, 7};
    const std::size_t actual_index = animated.bone_indices[parts[hand]];
    if (actual_index >= globals.size()) {
        return rs::make_err(std::string("Off-board pose missing actual animation hand"));
    }
    const auto& actual = globals[actual_index];
    if (reparented_hands[hand] >= globals.size()) {
        return rs::make_err(std::string("Off-board pose missing board-parented animation hand"));
    }
    const auto& reparented = globals[reparented_hands[hand]];
    animated.board_offset.refresh_transform(native::adjustment(actual, reparented));
    return ok();
}

}  // namespace s3::game::physics::offboard::pose_adjust
