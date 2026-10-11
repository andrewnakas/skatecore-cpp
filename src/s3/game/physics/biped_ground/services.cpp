// Port of skate-game/src/physics/biped_ground/services.rs
//! Concrete, stateless Ground bindings to the single SkaterRuntime owners.
//! Grab-spline queries/actions are a separate missing host, not board possession.
#include "s3/game/physics/biped_ground/services.hpp"

#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/player/offboard/air_selector.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/biped_ground.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::biped_ground::services {

using ::s3::animation::foot_ik::status::Mode;
using ::s3::physics::skeleton_animation_record::compose_affine;
namespace air_selector = ::s3::player::offboard::air_selector;

///82D770D8; S2 OffBoardFeetIK::ConsiderReset82DCD110 corroborates the owner.
///Do not reset targets/history merely because the board has been released.
void enter_feet(SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    if (!(p.state_2504 == 500 || p.state_2504 == 501 || p.state_2504 == 502)) {
        skater.offboard_feet.reset();
        skater.foot_ik.state.enable_feet(false);
        for (auto& limb : skater.foot_ik.state.limbs) {
            limb.board_blend = 0.0f;
            limb.external_blend = 0.0f;
            limb.mode = Mode::Disabled;
        }
    }
    skater.offboard_feet.flags_304_to_307[0] = p.state_2508 == 502;
}

///82D773F8 reads real line tests and82BE3220 world animation-record bones.
///Same input extraction as air_feet; neither its timer reset nor Air update
///is executed. Ground conditioning/history belong to super::feet::update.
feet::Input feet_input(const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    const auto& animation = skater.animated_skeleton;
    const auto root = animation.roots.animation_to_world;
    const auto effective = entry::effective_root(root, p.flags_2476);
    feet::Input input{};
    for (std::size_t i = 0; i < 2; ++i) {
        const auto line = p.line_tests_960_1008_1056[i];
        input.lines[i] = feet::Line{rs::from_bits(line.position), rs::from_bits(line.normal), line.surface,
                                    line.valid != 0};
    }
    input.root = root;
    input.inverse_root = animation.roots.world_to_animation;
    input.effective_root = effective;
    const std::array<std::size_t, 2> bones{15, 19};
    for (std::size_t k = 0; k < 2; ++k) {
        const std::size_t i = bones[k];
        input.local_foot_pairs[k] = {animation.record.pose[i][3], animation.record.pose[i + 1][3]};
    }
    for (std::size_t k = 0; k < 2; ++k) {
        const std::size_t i = bones[k];
        input.world_foot_pairs[k] = {compose_affine(root, animation.record.pose[i])[3],
                                     compose_affine(root, animation.record.pose[i + 1])[3]};
    }
    input.position = rs::from_bits(p.vectors_544_560_592_608[2]);
    input.velocity = rs::from_bits(p.vectors_544_560_592_608[3]);
    input.flags_2476 = p.flags_2476;
    input.flags_2480 = p.flags_2480;
    input.flags_2484 = p.flags_2484;
    input.state = p.state_2508;
    return input;
}

void update_feet(SkaterRuntime& skater) {
    auto input = feet_input(skater);
    biped_ground::feet::update(skater.offboard_feet, input, skater.foot_ik.state);
}

rs::Result<LaunchInput, std::string> launch_input(const SkaterRuntime& skater,
                                                   std::optional<air_launch::DepartureGeometry> geometry) {
    const auto& p = skater.player_input.processed;
    S3G_TRY_OPT(const auto* board, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "Ground launch requires the completed BoardToolkit");
    LaunchInput out{};
    auto& pr = out.processed;
    pr.board_position_112 = board->deck[3];
    pr.forward_224 = rs::from_bits(p.effective_anim_transform_192[2]);
    pr.up_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
    pr.position_592 = rs::from_bits(p.vectors_544_560_592_608[2]);
    pr.velocity_608 = rs::from_bits(p.vectors_544_560_592_608[3]);
    pr.velocity_912 = rs::from_bits(p.vectors_880_896_912_928_944[2]);
    pr.departure_geometry = geometry;
    pr.flags_2472 = p.flags_2472;
    pr.flags_2476 = p.flags_2476;
    pr.flags_2480 = p.flags_2480;
    pr.previous_state_2504 = p.state_2504;
    pr.current_state_2508 = p.state_2508;
    pr.current_category_2512 = p.category_2512;
    pr.previous_category_2516 = p.category_2516;
    pr.raw_x_2692 = skater.animation_input.extra.biped_world_x;
    pr.raw_z_2688 = skater.animation_input.extra.biped_world_z;
    out.elapsed_2664 = p.state_timer_2664;
    out.skeleton_point_10960 = skater.animated_skeleton.record.centre_of_mass;
    return out;
}

///82D32088..32108, after Skeleton has published its real processed fields.
void sync_grab(SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    const auto bone = compose_affine(skater.animated_skeleton.roots.animation_to_world,
                                     skater.animated_skeleton.record.pose[23])[3];
    grab::Input input{};
    for (std::size_t i = 0; i < 4; ++i) input.frame[i] = rs::from_bits(p.effective_anim_transform_192[i]);
    input.position = rs::from_bits(p.vectors_544_560_592_608[2]);
    input.bone = bone;
    input.flags_2476 = p.flags_2476;
    input.flags_2480 = p.flags_2480;
    input.flags_2484 = p.flags_2484;
    input.elapsed = p.state_timer_2664;
    input.timer_2852 = p.secondary_ground_timer_2852;
    input.context = ::s3::player::offboard::ground_query::QueryContext{
        p.actor_query_2948, static_cast<std::int32_t>(p.actor_query_2952)};
    grab::sync(skater.biped_ground.ground, skater.offboard_grab, skater.biped_ground.grab_settings, input);
}

///82D6CA58 copies the actual gravity vector and submits; consumption remains
///at the shared selector's native phase, not an invented Ground frame delay.
Status submit_air(GamePhysics& physics, SkaterRuntime& skater, air_launch::Packet packet) {
    const auto& p = skater.player_input.processed;
    const auto gravity = physics.settings.step.simulation.gravity_acceleration;
    air_selector::Context context{};
    context.selection_flags_2948 = p.actor_query_2948;
    context.matching_group_2952 = static_cast<std::int32_t>(p.actor_query_2952);
    context.up_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
    context.forward_224 = rs::from_bits(p.effective_anim_transform_192[2]);
    auto r = skater.offboard_air_selector.launch(physics.world, packet, {gravity.x, gravity.y, gravity.z, 0.0f},
                                                 context);
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return ok();
}

}  // namespace s3::game::physics::biped_ground::services
