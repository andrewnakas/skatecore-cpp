// Port of skate-game/src/physics/biped_air/update.rs
//! Ordered82D2EF38, including actual skeleton/selector/landing consumers.
#include "s3/game/physics/biped_air/update.hpp"

#include <utility>

#include "s3/core/player/offboard/biped_air/height.hpp"
#include "s3/core/player/offboard/biped_air/recovered.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/biped_air/input.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/offboard/air_feet.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"
#include "s3/game/physics/offboard/skeleton_air.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"

namespace s3::game::physics::biped_air::update {

namespace recovered = ::s3::player::offboard::biped_air::recovered;
using recovered::DT;
using recovered::height::projected_height;

Status update(GamePhysics& physics, SkaterRuntime& skater, const PlayerControls& /*controls*/) {
    const auto p = skater.player_input.processed;
    S3G_CHECK(skater.ground_lifecycle.skeleton_controller.request((p.flags_2484 & 2u) != 0 ? 7 : 4,
                                                                  skater.skeleton_collision));
    const std::int32_t frame = skater.biped_air.state.begin_update();
    auto feet = std::exchange(skater.offboard_feet, {});
    offboard::air_feet::update_air(skater, feet);
    skater.offboard_feet = feet;
    //82C040F0(0): shared board steering helper, not Ground locomotion.
    skater.ground.steering.update(0.0f, skater.air_settings.steering_blend, p.flags_2468, p.flags_2472);
    //82D2EFF0/82D2F014 load processed+0xB40 (AnimEndCOM), not
    //AnimTrans at0xB30. These are distinct stock animation attributes.
    if (const auto adjustment =
            skater.biped_air.state.animation_adjustment(skater.animation_input.fields.animation_end_com)) {
        skater.offboard_air_selector.adjust_animation(
            static_cast<std::int32_t>(static_cast<std::uint32_t>(frame) - 1u), *adjustment,
            skater.biped_air.state.frame_144);
    }
    skater.biped_air.state.result.reset();
    skater.offboard_air_selector.sample(frame, DT, skater.biped_air.state.result);
    skater.biped_air.state.orient_sample(skater.animation_input.extra.biped_start_angle, p.flags_2476);
    const auto up = rs::from_bits(p.vectors_544_560_592_608[0]);
    const auto position = rs::from_bits(p.vectors_544_560_592_608[2]);
    const auto response = skater.biped_air.state.collision_response(
        skater.collision_extra_errors, up, skater.offboard_air_selector.core.sampling.restart_allowed_8493);
    if (response.request_52) {
        skater.wipeout.state.request(32, 0.0f);
    }
    if (response.restart) {
        const auto normal = *response.restart;
        S3G_TRY(const auto packet0, input::launch_packet(skater));
        const auto feet_h = input::height(skater);
        const float height = projected_height(feet_h.bone15, feet_h.bone19, position, up);
        const auto packet = skater.biped_air.state.restart_packet(packet0, normal, position, up, height);
        S3G_CHECK(input::submit(physics, skater, packet));
        const auto effective = input::effective_frame(skater);
        skater.biped_air.state.finish_restart(effective, normal);
    }
    skater.biped_air.state.correct_restarted_sample();
    const auto height = input::height(skater);
    skater.biped_air.state.correct_height(height);
    const auto context = input::context(skater);
    {
        auto r = skater.offboard_air_selector.requery(physics.world, context, p.flags_2472, p.flags_2488);
        if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    }
    S3G_CHECK(skeleton(physics, skater));
    skater.biped_air.state.update_cadence(skater.biped_ground.controller.state,
                                          skater.animation_input.fields.cadence_end_percent);
    auto scene_r = offboard::contact_toolkit::StaticScene::new_(physics.world);
    if (scene_r.is_err()) return rs::make_err(std::string(scene_r.unwrap_err()));
    const auto& scene = scene_r.unwrap();
    S3G_CHECK(skater.landing_deck.assist(scene, skater.player_input,
                                         recovered::State::landing_assist_limit(p.state_timer_2664)));
    skater.biped_air.state.finish_landing_latch();
    return ok();
}

Status skeleton(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "BipedAir Skeleton requires the completed BoardToolkit");
    const auto board_forward = toolkit->deck[2];
    const auto collision = input_phase::collision(skater);
    const auto& state = skater.biped_air.state;
    offboard::skeleton_air::Input input{};
    input.frame_208 = state.frame_208;
    input.trajectory_position_272 = state.result.position_272;
    input.body_target_416 = state.body_target_416;
    input.lift_436 = state.body_offset_436;
    input.board_forward_96 = board_forward;
    skeleton_input_runtime::SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,   skater.skeleton_drives,         skater.foot_ik,
        skater.animation_input,   skater.skeleton_output.correction, skater.pose_errors,
    };
    //82BDDFFC..82BDE010 calls the SAME82D8E3E0 as Ground.
    //Only this common helper is borrowed; Air owns all movement/root updates.
    const auto& reckoning = skater.biped_ground;
    auto& air_reckoning = skater.air_reckoning;
    auto& riding = physics.riding;
    S3G_CHECK(skater.skeleton_input.update_biped_air(
        skater.skeleton_air, physics.board, input, skater.player_input.processed, owners,
        skater.animation.packet.hierarchy, collision, physics.settings.step.simulation,
        [&](offboard::skeleton_ground::ReckoningUpdate update,
            const ::s3::player::input_phase::ProcessedPhysicsInput& processed, float body_spin) -> Status {
            (void)reckoning.finish_reckoning(update, riding, air_reckoning, processed, body_spin);
            return ok();
        }));
    return ok();
}

}  // namespace s3::game::physics::biped_air::update
