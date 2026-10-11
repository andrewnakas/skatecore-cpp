// Port of skate-game/src/physics/wipeout_states/lifecycle.rs
//! Original Wipeout Enter82D3B5E8 and Exit82D3B990.
#include "s3/game/physics/wipeout_states/lifecycle.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/wipeout_states.hpp"

namespace s3::game::physics::wipeout_states::lifecycle {

namespace body = ::s3::player::wipeout_state::body;
namespace math = ::s3::player::wipeout_state::math;
namespace drives = ::s3::player::wipeout_state::drives;
using ::s3::math::Vector3;
using ::s3::physics::board::BodyId;
using ::s3::physics::contact::RetailContactMaterial;
using ::s3::player::wipeout_state::State;

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    skater.ground_lifecycle.skeleton_elapsed_16505 = false;
    auto& runtime = skater.wipeout_state;
    runtime.state = State{};
    runtime.contact.reset();
    runtime.prediction.reset();
    physics.board_wiping_out = true;
    physics.board.set_collision_group(7);
    physics.board.bodies_mut()[::s3::physics::board::index(BodyId::Deck)].inertia.angular_drag = 0.0f;
    RetailContactMaterial material{};
    material.static_friction = runtime.settings.board_friction;
    material.dynamic_friction = runtime.settings.board_friction;
    material.restitution = runtime.settings.board_restitution;
    physics.settings.wheel_material = material;
    physics.settings.truck_material = material;
    physics.settings.deck_material = material;
    skater.ground.steering.targets = {0.0f, 0.0f};
    auto& drive = physics.board.hook_mut().drive;
    drive.disable_angular();
    drive.disable_linear();
    //Do not reset the target history or the other limb ownership flags.
    for (auto& limb : skater.foot_ik.state.limbs) {
        limb.board_blend = 0.0f;
        limb.external_blend = 0.0f;
        limb.mode = ::s3::animation::foot_ik::status::Mode::Disabled;
    }
    skater.foot_ik.state.enable_feet(false);
    drives::set_angular_root(skater.skeleton_drives, runtime.drives, 0.5f);
    drives::set_linear_root(skater.skeleton_drives, runtime.drives, 0.0f);
    const auto& p = skater.player_input.processed;
    if ((p.flags_2488 & 0x00200000u) != 0) {
        body::blend_velocity(skater.skeleton, rs::from_bits(p.vectors_880_896_912_928_944[4]));
    }
    if (skater.wipeout.state.reasons[6]) {
        const auto normal = rs::from_bits(p.vectors_544_560_592_608[0]);
        body::remove_normal_velocity(skater.skeleton, normal);
        for (auto& part : physics.board.bodies_mut()) {
            const Vector3 v = part.rates.linear_velocity;
            const std::array<float, 4> lanes{v.x, v.y, v.z, 0.0f};
            const auto next = math::sub(lanes, math::scale(normal, math::dot(normal, lanes)));
            part.rates.linear_velocity = Vector3::new_(next[0], next[1], next[2]);
        }
        physics.board.bodies_mut()[::s3::physics::board::index(BodyId::Deck)].rates.angular_velocity = Vector3::ZERO;
    }
    body::limit_velocity(skater.skeleton, 20.0f);
    if ((p.flags_2468 & 4u) != 0) {
        skater.skeleton.apply_part_displacement(12,
                                                math::scale(rs::from_bits(p.vector_1520), runtime.settings.push_force));
    }
    if (p.probe_1792.byte_104 != 0) {
        body::add_velocity(skater.skeleton, rs::from_bits(p.probe_1792.vector_80));
    }
    runtime.state.special_surface = (p.flags_2488 & 0x40000000u) != 0;
    runtime.state.surface_height = p.collision_scalar_2924;
    S3G_CHECK(runtime.ragdoll.request(skater.ground_lifecycle.skeleton_controller,
                                      runtime.state.special_surface ? 10u : 8u, skater.skeleton,
                                      skater.skeleton_joints, skater.skeleton_collision));
    for (auto& part : skater.skeleton.bodies_mut()) part.inertia.linear_drag = 0.0f;
    runtime.state.move_board = (p.flags_2472 & 0x800u) != 0;
    runtime.state.board_offset = rs::from_bits(p.vectors_720_784_800_816_832_864[3]);
    runtime.state.board_move_frames = 0;
    runtime.state.predicted_time = rs::f32::MAX;
    runtime.state.airborne_frames = 15;
    if ((p.flags_2468 & 8u) != 0) {
        body::set_velocity(skater.skeleton, rs::from_bits(p.vectors_544_560_592_608[3]));
    }
    runtime.state.velocity = skater.animated_skeleton.board_frames.com_velocity;
    return ok();
}

void exit(GamePhysics& physics, SkaterRuntime& skater) {
    physics.board_wiping_out = false;
    auto& runtime = skater.wipeout_state;
    physics.board.bodies_mut()[::s3::physics::board::index(BodyId::Deck)].inertia.angular_drag =
        runtime.settings.deck_angular_drag;
    physics.board.set_collision_group(4);
    physics.settings.wheel_material = runtime.settings.standard_materials[0];
    physics.settings.truck_material = runtime.settings.standard_materials[1];
    physics.settings.deck_material = runtime.settings.standard_materials[2];
    runtime.state.prevent_manual = false;
}

void post_physics(SkaterRuntime& skater) {
    skater.wipeout_state.state.post_physics(skater.skeleton.record.velocities[23], skater.skeleton.record.velocities[1],
                                            skater.collision_feedback.flags.compliant,
                                            skater.wipeout_state.settings.recovery);
}

}  // namespace s3::game::physics::wipeout_states::lifecycle
