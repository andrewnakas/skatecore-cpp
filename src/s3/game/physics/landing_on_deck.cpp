// Port of skate-game/src/physics/landing_on_deck.rs
//! State503 lifecycle from original TU3 82D4D418..82D4E100.
#include "s3/game/physics/landing_on_deck.hpp"

#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/offboard/contact_toolkit.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::landing_on_deck {

using ::s3::physics::skeleton_animation_record::compose_affine;

rs::Result<Runtime, std::string> Runtime::load(const sd::Collections& data) {
    auto get = [&](const char* name) { return data.float_("physics_landingondeck", "default", name); };
    Runtime out{};
    S3G_TRY(out.settings.minimum_auto_angle, get("MinAngleToAutoTurn"));
    S3G_TRY(out.settings.automatic_speed, get("BodySpinSpeedAuto"));
    S3G_TRY(out.settings.input_speed, get("BodySpinSpeed"));
    S3G_TRY(out.settings.input_delta, get("BodySpinDeltaInput"));
    S3G_TRY(out.settings.automatic_delta, get("BodySpinDeltaAuto"));
    S3G_TRY(out.settings.maximum_landing_speed, data.float_("physics_wipeout", "default", "MaxSpeedLandingOnBoard"));
    S3G_TRY(out.root_settings.root_y_offset, data.float_("physics_skeleton", "default", "SkateRootYOffset"));
    S3G_TRY(out.root_settings.capsule_radius, data.float_("physics_skeleton", "default", "SkateRootCapsuleRadius"));
    S3G_TRY(out.root_settings.capsule_length, data.float_("physics_skeleton", "default", "SkateRootCapsuleLength"));
    return out;
}

void clear_ik(SkaterRuntime& skater) {
    for (auto& limb : skater.foot_ik.state.limbs) {
        limb.board_blend = 0.0f;
        limb.external_blend = 0.0f;
        limb.mode = ::s3::animation::foot_ik::status::Mode::Disabled;
    }
    skater.foot_ik.enable_feet(false);
}

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_CHECK(skater.ground_lifecycle.skeleton_controller.request(6, skater.skeleton_collision));
    const auto p = skater.player_input.processed;
    S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "Landing entry requires the completed board toolkit");
    const auto board = toolkit->deck;
    const auto hips = compose_affine(skater.animated_skeleton.roots.animation_to_world,
                                     skater.animated_skeleton.record.pose[23]);
    Entry entry{};
    entry.previous_category = p.category_2516;
    entry.hippy = (p.flags_2480 & 0x1000u) != 0;
    entry.strength = skater.animation_input.extra.jump_strength;
    entry.board_position = board[3];
    entry.com_position = rs::from_bits(p.vectors_544_560_592_608[2]);
    entry.com_velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    entry.board_velocity = rs::from_bits(p.vectors_400_416[0]);
    entry.up = rs::from_bits(p.vectors_544_560_592_608[0]);
    entry.hips_up = hips[1];
    entry.animation_right = rs::from_bits(p.effective_anim_transform_192[0]);
    entry.reversed = (p.flags_2476 & 4u) != 0;
    skater.landing_on_deck.state.enter(skater.landing_deck.manager, entry);
    clear_ik(skater);
    {
        auto effects = skater.board_possession_live.effects(physics, skater.ground_lifecycle.board_animated_290,
                                                            p.timestep_2604);
        effects.disable_animation();
        effects.standard_board();
    }
    skater.board_possession_live.publish_volumes(physics);
    if (skater.landing_on_deck.state.hippy) {
        skater.skeleton_output.trigger_wobble(false, (((p.flags_2468 >> 20) ^ (p.flags_2476 >> 2)) & 1u) != 0);
    }
    return ok();
}

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    const auto p = skater.player_input.processed;
    S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "Landing update requires the completed board toolkit");
    const auto board = toolkit->deck;
    const auto forward = toolkit->effective[2];
    auto scene_r = offboard::contact_toolkit::StaticScene::new_(physics.world);
    if (scene_r.is_err()) return rs::make_err(std::string(scene_r.unwrap_err()));
    const auto& scene = scene_r.unwrap();
    S3G_TRY(const auto output, skater.landing_deck.update(scene, skater.player_input));
    skater.landing_on_deck.state.align(skater.landing_on_deck.settings, forward,
                                       rs::from_bits(p.effective_anim_transform_192[2]), p.state_timer_2664,
                                       rs::from_bits(p.vectors_544_560_592_608[3][1]),
                                       skater.animation_input.extra.physical_body_spin);
    S3G_CHECK(skeleton::update(physics, skater, output.position));
    skater.landing_on_deck.state.advance_spin(output);
    const auto& manager = skater.landing_deck.manager;
    if (manager.trajectory_32.velocity_at(manager.elapsed_160)[1] < 0.0f) {
        const float time = State::accurate_time(
            board[3][1], rs::from_bits(p.vectors_400_416[0][1]), rs::from_bits(p.vectors_544_560_592_608[3][1]),
            {skater.skeleton.record.pose[15][3][1], skater.skeleton.record.pose[19][3][1]}, p.wheel_count_2556);
        skater.landing_on_deck.state.time_to_land = time;
        skater.landing_on_deck.state.near_deck = time < 0.016000001f;
        S3G_CHECK(skater.landing_deck.calculate_accurate_ik_offset(
            skater.player_input, skater.animated_skeleton, skater.animated_skeleton.unadjusted_board[3], time));
        if (skater.landing_on_deck.state.near_deck) {
            skater.skeleton_output.trigger_wobble(true, (((p.flags_2468 >> 20) ^ (p.flags_2476 >> 2)) & 1u) != 0);
        }
    }
    if (rs::abs(skater.animated_skeleton.unadjusted_board[2][2]) >= 0.97000003f) {
        if ((p.flags_2484 & 2u) == 0 && skater.landing_on_deck.state.time_to_land < 0.2f) {
            skater.foot_ik.enable_feet(true);
        }
    } else {
        clear_ik(skater);
    }
    skater.landing_on_deck.state.finish(skater.landing_on_deck.settings,
                                        rs::from_bits(p.vectors_544_560_592_608[3][1]));
    return ok();
}

///82D4DF08: synchronize actual query completion before rejection/collision tests.
Status post(SkaterRuntime& skater, Frame frame) {
    S3G_CHECK(skater.landing_deck.post_physics(skater.player_input));
    if (!skater.landing_deck.fill().can_land_316) {
        skater.wipeout.state.request(24, 0.0f);
    }
    if (skater.landing_on_deck.state.dangerous) {
        skater.wipeout.state.request(27, 0.0f);
    }
    return skater.wipeout.check_air_collision(skater.player_input.processed, frame);
}

///82D4D830 resets only the shared manager; outgoing state data is not another trajectory.
Status exit(GamePhysics& /*physics*/, SkaterRuntime& skater) {
    skater.landing_deck.reset();
    return ok();
}

}  // namespace s3::game::physics::landing_on_deck
