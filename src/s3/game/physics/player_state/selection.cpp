// Port of skate-game/src/physics/player_state/selection.rs.
//! CalcSuggestedState82D8ADE8 gets actual contacts, line distances and pose.
#include "s3/game/physics/player_state/selection.hpp"

#include "s3/core/player/selector.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/player_state/transition.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::player_state::selection {

using sc::player::selector::conditions::BoardBodyState;
using sc::player::selector::conditions::SkeletonAnimationState;
using sc::player::selector::input::ProcessedStateInput;
using sc::player::selector::input::StateSelectionInput;

Status advance(GamePhysics& physics, SkaterRuntime& skater, sc::player::input_phase::ProcessedPhysicsSnapshot snapshot) {
    // debug_assert_eq!(snapshot.tick, physics.ticks);
    const auto& p = snapshot.input;
    StateSelectionInput input{};
    ProcessedStateInput& pr = input.processed;
    pr.grind_type_1248 = p.grind.family_1248;
    pr.grind_candidate_1488 = p.grind.valid_1488;
    pr.grind_investigation_flags_1516 = p.grind.flags_1516;
    pr.field_1776 = static_cast<std::int32_t>(p.external_physics_1616.flags);
    pr.flags_2468 = p.flags_2468;
    pr.flags_2472 = p.flags_2472;
    pr.flags_2476 = p.flags_2476;
    pr.flags_2480 = p.flags_2480;
    pr.flags_2484 = p.flags_2484;
    pr.flags_2488 = p.flags_2488;
    pr.category_2512 = p.category_2512;
    pr.wheel_contact_count_2556 = static_cast<std::int32_t>(p.wheel_count_2556);
    pr.field_2572 = skater.player_state.post.state_frames;
    pr.state_timer_2664 = p.state_timer_2664;
    pr.field_2732 = skater.animation_input.fields.slide;
    pr.field_2744 = skater.animation_input.extra.revert_direction;
    pr.trajectory_collision_time_2772 = p.air_scalar_2772;
    input.skateboard_contact_count_869 = physics.riding.ground.wheel_contact_count;
    input.board_body = BoardBodyState{physics.riding.wheel_lines.minimum_distance,
                                      physics.riding.ground.time_without_wheel_contact};
    SkeletonAnimationState skeleton{};
    skeleton.mode_16420 = skater.skeleton_input.force_mode;
    skeleton.back_chain_lane_12656 = skater.skeleton_input.drive_frames[0][2][1];
    skeleton.threshold_800 = skater.player_state.animated_board_threshold;
    input.skeleton = skeleton;
    input.normal_off_ground = skater.player_state.normal_off_ground;
    input.skitching_off_ground = skater.player_state.skitching_off_ground;
    const auto current = skater.player_state.current();
    const auto requested = skater.player_state.selector.calculate(current, input);
    skater.player_state.requested_state = requested;
    if (requested != current) {
        // 82DB5FF4 restores the actual counter also used by ground-history.
        skater.player_input.player.ground_history_frames_1304 = 100;
        S3G_CHECK(transition::set(physics, skater, requested));
    }
    // SystemLogic82DB6000 clears the per-frame Wipeout request accumulator.
    skater.wipeout.state.clear_after_selection();
    return ok();
}

}  // namespace s3::game::physics::player_state::selection
