// Port of skate-game/src/physics/ground_animation/board.rs.
//!82D33E30: jump launch or ordered foot/brake/friction/collision forces.
#include "s3/game/physics/ground_animation/board.hpp"

#include "s3/core/air/ground_jump.hpp"
#include "s3/core/riding/braking.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_force.hpp"
#include "s3/core/riding/grounded/drag.hpp"
#include "s3/core/riding/slide_friction.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::ground_animation::board {

namespace ground_jump = sc::air::ground_jump;
namespace braking = sc::riding::braking;
namespace ground_force = sc::riding::ground_force;
namespace slide_friction = sc::riding::slide_friction;
using sc::physics::force_queue::QueuedPointForce;
using sc::riding::grounded::drag::DRAG_FREQUENCY;

void set_drag(GamePhysics& physics, float drag) {
    //BodySetLinearDrag82D9CD60 multiplies by native59.999996, for all seven parts.
    for (auto& body : physics.board.bodies_mut()) {
        body.inertia.linear_drag = drag * DRAG_FREQUENCY;
    }
}

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("GroundAnimation requires current BoardToolkit"));
    }
    const auto t = *skater.player_input.toolkit;
    const auto s_ptr = skater.ground_settings;  // keeps the Arc alive across borrows
    const auto& s = *s_ptr;
    skater.ground.steering.update(0.0f, s.board().steering.tilt_blending, p.flags_2468, p.flags_2472);
    physics.settings.wheel_material = s.wheel_material;
    if (std::size_t(p.state_variant_index_2528) >= skater.ground_animation_settings.modes.size()) {
        return rs::make_err(fmt::cat("Invalid GroundAnimation physics mode ", p.state_variant_index_2528));
    }
    auto mode = skater.ground_animation_settings.modes[p.state_variant_index_2528];
    mode.minimum_height_64 *= physics.trainer.pop;
    mode.minimum_height_68 *= physics.trainer.pop;
    mode.maximum_height *= physics.trainer.pop;
    ground_jump::GroundJumpInput jin;
    jin.flags_2468 = p.flags_2468;
    jin.flags_2480 = p.flags_2480;
    jin.flags_2484 = p.flags_2484;
    jin.flags_2488 = p.flags_2488;
    jin.effective_forward = t.effective[2];
    jin.forward = t.forward;
    jin.current_velocity = rs::from_bits(p.vectors_400_416[0]);
    jin.ground_reference_position = rs::from_bits(p.vectors_464_480_496_512_528[1]);
    jin.reference_up = rs::from_bits(p.vectors_544_560_592_608[0]);
    jin.filtered_ground_normal = t.filtered_normal;
    jin.animation_com_position = rs::from_bits(p.vectors_544_560_592_608[2]);
    jin.prepared_velocity = rs::from_bits(p.prepared_jump_704);
    jin.jump_strength = skater.animation_input.extra.jump_strength;
    jin.jump_controls = skater.animation_input.extra.jump_controls;
    jin.gravity_y = p.gravity_2648;
    jin.surface_speed = p.scalar_2656;
    skater.ground_animation.jump = ground_jump::calculate(jin, mode, skater.ground_animation_settings.jump);
    if (skater.ground_animation.jump.active) {
        const auto velocity = skater.ground_animation.jump.velocity;
        skater.ground_runtime.set_animated_velocity(physics.board, velocity);
        skater.ground_animation.launched = true;
        set_drag(physics, 0.0f);
        S3G_TRY(auto info, air_phase::launch_info(physics, skater));
        info.start_velocity = velocity;
        info.player_jumped = true;
        //The native caller does not replace Fill's COM velocity144.
        S3G_TRY(const auto input, air_phase::selector_input(physics, skater));
        S3G_CHECK(skater.trajectory.launch(info, input, physics.world));
        if (!skater.player_input.toolkit) {
            return rs::make_err(std::string("Jump trajectory requires current board toolkit"));
        }
        const auto grind_context = air_trajectory::GrindContext::from_processed(skater.player_input.processed,
                                                                                skater.player_input.toolkit->deck[3]);
        S3G_CHECK(skater.trajectory.update(input, physics.world, grind_context));
        //Launch82D678DC stores packet at1920;128 is selector2048. Update's
        //second pass may replace that retained launch velocity before this read.
        const auto* launch = skater.trajectory.selector.launch_info();
        if (launch == nullptr) {
            return rs::make_err(std::string("GroundAnimation launch has no retained trajectory packet"));
        }
        skater.ground_animation.launch_velocity = launch->start_velocity;
        return ok();
    }
    const auto settings = s.board();
    const float balance = skater.animation_input.fields.balance;
    const float front_scale = balance <= 0.0f ? (balance >= -0.0f ? 1.0f : 2.0f) : 0.0f;
    const float rear_scale = balance >= -0.0f ? (balance <= 0.0f ? 1.0f : 2.0f) : 0.0f;
    auto foot = [&](float offset, float absorption, float amount) {
        ground_force::GroundForceInput gi;
        gi.argument_1 = 1.0f;
        gi.application_z = offset;
        gi.argument_3 = absorption;
        gi.argument_4 = amount;
        gi.balance = balance;
        gi.surface_speed = p.scalar_2656;
        gi.axis_384 = t.transverse_up;
        gi.velocity_400 = rs::from_bits(p.vectors_400_416[0]);
        gi.axis_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
        return ground_force::calculate(settings.ground_force, gi);
    };
    const auto front = foot(s.foot_force_offset, 0.0f, front_scale);
    //Toolkit cache1240 is FootForceOffset,1236 is AbsorptionFootForceRearScalar.
    const auto rear = foot(-s.foot_force_offset, s.absorption_rear * 0.0f, rear_scale);
    braking::BrakeInput bi;
    bi.flags_2468 = p.flags_2468;
    bi.input_2728 = skater.animation_input.fields.brake;
    bi.signed_speed = p.scalar_2612;
    bi.absolute_body_speed = t.absolute_speed;
    bi.surface_factor = s.surface_braking_factor;
    bi.direction = xyz(t.horizontal_forward);
    const auto brake = braking::calculate_braking(bi, settings.propulsion.braking);
    slide_friction::SlideFrictionInput si;
    si.heading_time = p.state_timer_2664;
    si.normal = rs::from_bits(p.vectors_464_480_496_512_528[0]);
    si.velocity = rs::from_bits(p.vectors_400_416[0]);
    si.side_axis = t.deck[0];
    si.surface_speed = p.scalar_2656;
    si.scalar_2764 = p.scalar_2764;
    const auto friction = slide_friction::calculate(settings.slide_friction, si);
    const auto normal = physics.riding.reckoning.ground_normal;
    braking::LinearDragInput li;
    li.flags_2468 = p.flags_2468;
    li.absolute_body_speed = t.absolute_speed;
    li.balance_2720 = balance;
    li.scalar_2724 = skater.ground_lifecycle.manual_drag_2724;
    li.comparison_scalar = normal.y;
    const float drag = braking::calculate_linear_drag(li, settings.linear_drag);
    sc::riding::collision_response::CollisionResponsePhysical cp;
    cp.flags_2472 = p.flags_2472;
    cp.collision_displacement = rs::from_bits(p.collision_pose_error_736);
    cp.velocity = rs::from_bits(p.vectors_400_416[1]);
    cp.forward = t.travel_direction;
    cp.up = rs::from_bits(p.vectors_544_560_592_608[0]);
    cp.ground_normal = {normal.x, normal.y, normal.z, 0.0f};
    cp.time_step = p.timestep_2604;
    cp.mass = t.total_mass;
    const auto collision = skater.ground_runtime.calculate_collision_force(cp);
    if (collision) {
        physics.board.forces_mut().append(QueuedPointForce{
            15,
            xyz(collision->force_2528),
            xyz(collision->point_2544),
        });
        //Collision branch preserves the preceding body's drag coefficient.
    } else {
        physics.board.forces_mut().append(point_force(4, front));
        physics.board.forces_mut().append(point_force(5, rear));
        physics.board.forces_mut().append(point_force(1, friction));
        physics.board.forces_mut().append(brake);
        set_drag(physics, drag);
    }
    return ok();
}

QueuedPointForce point_force(std::uint32_t tag, std::array<float, 8> v) {
    return QueuedPointForce{
        tag,
        sc::math::Vector3::new_(v[0], v[1], v[2]),
        sc::math::Vector3::new_(v[4], v[5], v[6]),
    };
}

sc::math::Vector3 xyz(std::array<float, 4> v) { return sc::math::Vector3::new_(v[0], v[1], v[2]); }

}  // namespace s3::game::physics::ground_animation::board
