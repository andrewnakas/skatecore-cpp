// Port of skate-game/src/physics/grind/substate/nonspecific.rs.
#include "s3/game/physics/grind/substate/nonspecific.hpp"

#include "s3/core/player/selector/conditions.hpp"
#include "s3/core/riding/anti_flip.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::grind::substate::nonspecific {

using LaunchInput = ::s3::physics::grind_forces::launch::Input;
using ::s3::player::selector::conditions::is_skateboard_animated;
using ::s3::player::selector::conditions::SkeletonAnimationState;
namespace anti_flip = ::s3::riding::anti_flip;

Status execute(GamePhysics& physics, SkaterRuntime& skater) {
    skater.grind.nonspecific_jumped = false;
    const auto& p = skater.player_input.processed;
    if ((p.flags_2468 & 0x00400000u) != 0) {
        if (!skater.player_input.toolkit) {
            return rs::make_err(std::string("Nonspecific pop requires current BoardToolkit"));
        }
        const auto position = skater.player_input.toolkit->deck[3];
        LaunchInput input;
        input.velocity_400 = rs::from_bits(p.vectors_400_416[0]);
        input.position_112 = position;
        input.current_point_1120 = rs::from_bits(p.grind.point_1120);
        input.balance_2800 = skater.animation_input.extra.grind_stability_nudge;
        input.geometry_side_jump = rs::from_bits(0x3f333333u);
        S3G_TRY(const float vertical, skater.grind.settings.substate.vertical(Family::FiftyFifty,
                                                                            p.state_variant_index_2528,
                                                                            skater.animation_input.extra.jump_strength));
        input.vertical_jump = vertical * physics.trainer.grind_pop;
        const auto velocity = launch::apply(physics.board, skater.player_input.grind.jumper,
                                            skater.player_input.processed.flags_2476, input);
        skater.grind.nonspecific_jump_velocity = velocity;
        skater.grind.nonspecific_jumped = true;
        S3G_CHECK(skeleton::animated(physics, skater));
        //82D42E8C reapplies velocity AFTER animated board/skeleton update.
        skater.ground_runtime.set_animated_velocity(physics.board, velocity);
    } else {
        SkeletonAnimationState animation_state;
        animation_state.mode_16420 = skater.skeleton_input.force_mode;
        animation_state.back_chain_lane_12656 = skater.skeleton_input.drive_frames[0][2][1];
        animation_state.threshold_800 = skater.grind.settings.substate.animated_board_threshold;
        const bool animated = is_skateboard_animated(animation_state);
        std::optional<skeleton::AnimationPartTransform> target_opt;
        if (animated && (p.flags_2472 & 0x00400000u) != 0) {
            S3G_TRY(const auto t, skeleton::animated_target(physics, skater));
            target_opt = t;
        } else {
            S3G_TRY(const auto t, skeleton::ground_target(physics, skater));
            target_opt = t;
        }
        const skeleton::AnimationPartTransform target = *target_opt;
        //82D42F50 starts with steering0 then82C04368, before angular/support.
        const auto& p2 = skater.player_input.processed;
        skater.ground.steering.update(0.0f, skater.ground_settings->board().steering.tilt_blending, p2.flags_2468,
                                      p2.flags_2472);
        skater.skeleton_air.capture_physics_error(physics.board, target);
        if (!skater.player_input.toolkit) {
            return rs::make_err(std::string("Nonspecific board update requires current BoardToolkit"));
        }
        const auto& toolkit = *skater.player_input.toolkit;
        const auto up = rs::from_bits(p2.vectors_544_560_592_608[0]);
        anti_flip::AntiFlipInput afi;
        afi.flags_2468 = p2.flags_2468;
        afi.balance = skater.animation_input.fields.balance;
        afi.axis_96 = toolkit.deck[2];
        afi.axis_64 = toolkit.deck[0];
        afi.projection_axis_544 = up;
        const auto correction = anti_flip::calculate(skater.ground_settings->board().anti_flip, afi);
        //82C075B8 adds torque acceleration, not angular velocity/orientation.
        skater.ground_runtime.apply_angular_displacement(physics.board, correction);
        board::apply_world_force(physics.board, {up[0] * -40.0f, up[1] * -40.0f, up[2] * -40.0f, up[3] * -40.0f},
                                 toolkit.deck[3]);
        //Unlike common grind,701 discards optional velocity and torque outputs.
        S3G_CHECK(collision_force(physics, skater));
    }
    return ok();
}

}  // namespace s3::game::physics::grind::substate::nonspecific
