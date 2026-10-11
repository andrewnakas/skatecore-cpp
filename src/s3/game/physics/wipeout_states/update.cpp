// Port of skate-game/src/physics/wipeout_states/update.rs
//! Complete Wipeout300 update order82D3B9C8, on the game body/pose owners.
#include "s3/game/physics/wipeout_states/update.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/skeleton_input_runtime.hpp"
#include "s3/game/physics/wipeout_states.hpp"

namespace s3::game::physics::wipeout_states::update {

namespace ws = ::s3::player::wipeout_state;
using ::s3::math::Vector3;
using ::s3::physics::board::BodyId;

namespace {
const float DT = rs::from_bits(0x3C888889u);
Vector3 vector(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }
}  // namespace

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    auto& runtime = skater.wipeout_state;
    auto& state = runtime.state;
    const bool contact = skater.collision_feedback.flags.any || state.below_surface;
    state.airborne_frames =
        contact ? -1 : std::int32_t(std::uint32_t(state.airborne_frames) + 1u);  // wrapping_add
    if (!state.special_surface && (p.flags_2488 & 0x40000000u) != 0) {
        state.special_surface = true;
        state.surface_height = p.collision_scalar_2924;
        S3G_CHECK(runtime.ragdoll.request(skater.ground_lifecycle.skeleton_controller, 10, skater.skeleton,
                                          skater.skeleton_joints, skater.skeleton_collision));
    }
    const auto velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    if (velocity[1] > 5.0f && velocity[1] - state.velocity[1] > 5.0f) {
        ws::body::limit_velocity(skater.skeleton, 5.0f);
    }
    state.velocity = velocity;
    const float speed = ws::math::length(velocity);
    ws::response::retained_velocity(state, skater.skeleton, skater.skeleton_drives, runtime.drives, contact,
                                    p.flags_2468, velocity);
    const auto position = rs::from_bits(p.vectors_544_560_592_608[2]);
    if (state.special_surface) {
        state.below_surface = position[1] - state.surface_height < 0.0f;
        ws::body::special_surface(skater.skeleton, state.surface_height);
    }
    ::s3::physics::skeleton_animation_record::AnimationPartTransform effective{};
    for (std::size_t i = 0; i < 4; ++i) effective[i] = rs::from_bits(p.effective_anim_transform_192[i]);
    const auto controls = skater.animation_input.extra.wipeout_control;
    const auto gesture = skater.animation_input.extra.wipeout_gesture;
    if (!state.below_surface) {
        S3G_CHECK(contact::update(state, runtime.contact, skater.skeleton, skater.collision_feedback,
                                  skater.ground_lifecycle.skeleton_controller, skater.skeleton_collision,
                                  skater.skeleton_joints, runtime.ragdoll, contact, velocity, effective[2], controls));
        float drag = 0.0f;
        if (state.slow && contact) {
            drag = state.over ? ws::math::clamp(state.settled_time * 0.15f + 0.05f, 0.0f, 0.5f) : 0.05f;
        }
        ws::body::set_wipeout_drag(skater.skeleton, drag);
    }
    state.time += DT;
    state.response_time += DT;
    state.extra_weight_zero_time += DT;
    if (state.extra_weight > 0.0f) state.extra_weight_zero_time = 0.0f;
    state.response_scalar = 0.0f;
    state.slow_time += DT;
    state.maximum_speed = speed - state.maximum_speed >= 0.0f ? speed : state.maximum_speed;
    if (speed > 2.0f) state.slow_time = 0.0f;
    ws::response::update_counter(state, contact);
    const auto com = skater.animated_skeleton.board_frames.centre_of_mass;
    bool control_applied = false;
    if (state.below_surface || state.imminent_surface_twelve) {
        ws::control_air::update(state, skater.skeleton, com, controls, true);
    } else if ((p.flags_2484 & 0x100u) != 0 && state.response_time >= 0.1f) {
        const auto& feedback = skater.collision_feedback;
        const auto normal = ws::response::response_normal(
            feedback.flags.compliant ? std::optional<std::array<float, 4>>(feedback.highest_normal) : std::nullopt,
            runtime.prediction.result.valid()
                ? std::optional<std::array<float, 4>>(runtime.prediction.result.landing_normal)
                : std::nullopt);
        ws::response::trigger(state, skater.skeleton, normal, controls);
    } else if (state.airborne_frames > 15 && !state.over) {
        ws::profiles::prepare(state, runtime.profiles, gesture, controls);
        const auto& profile = runtime.profiles[state.profile];
        ws::control_profile_air::update(state, skater.skeleton, com, effective, controls, profile);
        ws::profiles::drift(state, skater.skeleton, profile);
        control_applied = true;
    } else if ((!contact && state.airborne_frames > 15) || state.over) {
        control_applied = ws::control_air::update(state, skater.skeleton, com, controls, contact);
    } else {
        ws::profiles::prepare(state, runtime.profiles, gesture, controls);
        const auto predicted_position = state.predicted_position;
        ws::control_ground::update(state, skater.skeleton, com, effective, predicted_position,
                                   runtime.profiles[state.profile], skater.animated_skeleton.roots.animation_to_world,
                                   skater.animated_skeleton.record.pose);
        control_applied = true;
    }
    ws::weights::Settings weight_settings{};
    weight_settings.remove_target_time = runtime.settings.remove_target_time;
    weight_settings.remove_drives_time = runtime.settings.remove_drives_time;
    weight_settings.collision_step = runtime.settings.collision_weight_step;
    weight_settings.controlled_step = runtime.settings.controlled_weight_step;
    const auto weight = ws::weights::update(state, weight_settings, contact, control_applied, p.flags_2472, gesture);
    skater.ground.steering.update(0.0f, skater.air_settings.steering_blend, p.flags_2468, p.flags_2472);
    ws::drives::set_angular_root(skater.skeleton_drives, runtime.drives, weight.target_weight);
    skeleton_input_runtime::SkeletonOwners owners{
        skater.animated_skeleton, skater.skeleton,     skater.skeleton_drives,           skater.foot_ik,
        skater.animation_input,   skater.skeleton_output.correction, skater.pose_errors,
    };
    const auto& hierarchy = skater.animation.packet.hierarchy;
    S3G_CHECK(skeleton::update(skater.skeleton_input, owners, p, hierarchy.data(), hierarchy.size(), runtime.drives,
                               weight.start, weight.end, weight.controlled, weight.extra,
                               state.retained_velocity_active));
    if (state.move_board && std::int32_t(state.board_move_frames) < 21) {
        const Vector3 deck =
            physics.board.part_transforms()[::s3::physics::board::index(BodyId::Deck)].translation;
        S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                    "Wipeout board force requires the current BoardToolkit");
        const auto point = toolkit->deck[3];
        ::s3::riding::ground_correction_math::apply_world_force(
            physics.board.bodies_mut()[::s3::physics::board::index(BodyId::Deck)], deck, vector(state.board_offset),
            vector(point));
        state.board_move_frames = state.board_move_frames + 1u;
    }
    state.manage_recovery(p.flags_2468, p.flags_2472, p.flags_2484, p.timestep_2604, runtime.settings.recovery);
    const auto gravity = physics.settings.step.simulation.gravity_acceleration;
    return runtime.prediction.advance(state, physics.world, position, {gravity.x, gravity.y, gravity.z, 0.0f});
}

}  // namespace s3::game::physics::wipeout_states::update
