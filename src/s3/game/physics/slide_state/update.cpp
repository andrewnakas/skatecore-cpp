// Port of skate-game/src/physics/slide_state/update.rs.
//! Ordered UpdateSkateboard82D3A900: steering, manual, wall launch or Slide forces.
#include "s3/game/physics/slide_state/update.hpp"

#include "s3/core/physics/manual/controller.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_contact_response.hpp"
#include "s3/core/riding/ground_force.hpp"
#include "s3/core/riding/steering.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::slide_state::update_ {

namespace ground_force = sc::riding::ground_force;
namespace steering = sc::riding::steering;
using ground_runtime::GroundInputObservations;
using sc::physics::force_queue::QueuedPointForce;
using sc::player::slide_state::angular_correction;
using sc::player::slide_state::sliding_force;
using sc::player::slide_state::SlideInput;

Status board(GamePhysics& physics, SkaterRuntime& skater) {
    // The preceding Skeleton call captured the actual unblended board target.
    // Re-capture at Slide's own82D3A924 call; no tick elapses between these writes.
    const auto target = skater.animated_skeleton.board_frames.animation_target;
    skater.skeleton_air.capture_physics_error(physics.board, target);
    const auto& p = skater.player_input.processed;
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Slide requires current BoardToolkit"));
    }
    const auto& t = *skater.player_input.toolkit;
    if (p.surface_mode_2540 == 0) {
        return rs::make_err(std::string("Slide requires the selected SurfacePhysics profile"));
    }
    const std::size_t surface_index = std::size_t(p.surface_mode_2540 - 1);
    if (surface_index >= skater.slide_state.surfaces.size()) {
        return rs::make_err(fmt::cat("Invalid Slide surface mode ", p.surface_mode_2540));
    }
    const auto& [surface, material] = skater.slide_state.surfaces[surface_index];
    // All four actual wheel query volumes share the material copied from2552.
    physics.settings.wheel_material = material;
    const auto edge = skater.ground_lifecycle.edge;
    S3G_TRY(const auto pump_mode, skater.ground.pumping_settings.mode(p.state_variant_index_2528));
    GroundInputObservations extra;
    extra.manual_drag_2724 = skater.ground_lifecycle.manual_drag_2724;
    extra.trajectory_state_bits = p.external_physics_1616.flags;
    extra.edge_flags = edge ? edge->flags : 0u;
    extra.edge_point = edge ? edge->point : std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f};
    const auto ground_settings = skater.ground_settings;  // Arc clone keeps the profile alive
    auto input = ground_settings->input(t, p, skater.animation_input, skater.ground.pumping,
                                        pump_mode.unintentional_scalar, physics.riding, skater.animated_skeleton,
                                        physics.settings.step.base_truck_transforms, extra);
    const auto settings = ground_settings->board();
    auto& state = skater.slide_state.state;
    const float tilt =
        steering::calculate_tilt(settings.steering, input.steering, &state.steering_push, &state.damped_turn);
    skater.ground.steering.update(tilt, settings.steering.tilt_blending, p.flags_2468, p.flags_2472);
    input.manual.powersliding = true;
    Angle angle;
    const auto manual_r = sc::physics::manual::controller::calculate(skater.ground.manual, settings.manual,
                                                                     settings.manual_mode, input.manual, angle);
    if (manual_r.is_err()) {
        // ManualError<Infallible>: only the Angle variant is constructible.
        return rs::make_err(std::string("Slide manual controller: Angle(IntegerConversionUnavailable)"));
    }
    const auto manual_effect = manual_r.unwrap();
    const auto normal = raw(p.vectors_464_480_496_512_528[0]);
    const auto up = raw(p.vectors_544_560_592_608[0]);
    const auto velocity = raw(p.vectors_400_416[0]);
    auto contact_frame = input.contact;
    contact_frame.scalar_2756 = 0.0f;
    sc::riding::ground_contact_response::WallRidePhysical wall;
    wall.board_normal = normal;
    wall.up = up;
    wall.velocity = velocity;
    wall.board_mass = t.total_mass;
    wall.gravity = p.gravity_2648;
    wall.speed = p.scalar_2652;
    wall.contact_count = static_cast<std::int32_t>(p.wheel_count_2556);
    const auto response =
        skater.ground_runtime.contact_response_with_previous(contact_frame, wall, {0.0f, 0.0f, 0.0f, 0.0f});
    state.wall_riding = response.active_2731;
    if (response.animated_board_2708) {
        skater.ground_runtime.set_animated_velocity(physics.board, response.vector_2688);
        for (auto& body : physics.board.bodies_mut()) {
            body.inertia.linear_drag = 0.0f;
        }
        S3G_TRY(auto launch, air_phase::launch_info(physics, skater));
        launch.start_velocity = response.vector_2688;
        launch.player_jumped = true;
        S3G_TRY(const auto sel_input, air_phase::selector_input(physics, skater));
        S3G_CHECK(skater.trajectory.launch(launch, sel_input, physics.world));
        if (!skater.player_input.toolkit) {
            return rs::make_err(std::string("Slide trajectory requires current board toolkit"));
        }
        const auto grind_context = air_trajectory::GrindContext::from_processed(skater.player_input.processed,
                                                                                skater.player_input.toolkit->deck[3]);
        S3G_CHECK(skater.trajectory.update(sel_input, physics.world, grind_context));
        //82D3AB10 branches directly to the epilogue. No ordinary force tail.
        return ok();
    }
    SlideInput slide;
    slide.velocity = velocity;
    slide.normal = normal;
    slide.side = t.deck[0];
    slide.effective_forward = t.effective[2];
    slide.reference_forward = t.forward;
    slide.angular_velocity = raw(p.vectors_720_784_800_816_832_864[0]);
    slide.absolute_speed = t.absolute_speed;
    slide.surface_speed = p.scalar_2656;
    slide.slide = skater.animation_input.fields.slide;
    slide.elapsed = p.state_timer_2664;
    slide.wheel_hardness = p.scalar_2764;
    const auto displacement = angular_correction(skater.slide_state.settings, surface, slide);
    skater.ground_runtime.apply_angular_displacement(physics.board, displacement);
    std::array<float, 4> manual{};
    for (std::size_t i = 0; i < 4; ++i) {
        manual[i] = manual_effect.angular_displacement[i] * skater.slide_state.manual_scalar;
    }
    skater.ground_runtime.apply_angular_displacement(physics.board, manual);
    const auto& f = input.ground_force;
    const float balance = f.balance_2720;
    //82D3AEC0..AF10 preserves the strict scalar branches and fsel signs.
    const float front_factor = balance > 0.0f ? 0.0f : fsel(balance, 1.0f, 2.0f);
    const float rear_factor = fsel(balance, balance > 0.0f ? 2.0f : 1.0f, 0.0f);
    ground_force::GroundForceInput fi;
    fi.argument_1 = f.argument_1_2752;
    fi.application_z = f.ground_scalar_1216;
    fi.argument_3 = 0.0f;
    fi.argument_4 = front_factor;
    fi.balance = balance;
    fi.surface_speed = f.surface_speed_2656;
    fi.axis_384 = f.axis_384;
    fi.velocity_400 = f.velocity_400;
    fi.axis_544 = f.axis_544;
    const auto front = ground_force::calculate(settings.ground_force, fi);
    ground_force::GroundForceInput ri;
    ri.argument_1 = f.argument_1_2752;
    ri.application_z = -f.ground_scalar_1240;
    ri.argument_3 = f.ground_scalar_1236 * 0.0f;
    ri.argument_4 = rear_factor;
    ri.balance = balance;
    ri.surface_speed = f.surface_speed_2656;
    ri.axis_384 = f.axis_384;
    ri.velocity_400 = f.velocity_400;
    ri.axis_544 = f.axis_544;
    const auto rear = ground_force::calculate(settings.ground_force, ri);
    const auto sliding = sliding_force(skater.slide_state.settings, surface, slide);
    const auto n = physics.riding.reckoning.ground_normal;
    sc::riding::collision_response::CollisionResponsePhysical cp;
    cp.flags_2472 = p.flags_2472;
    cp.collision_displacement = raw(p.collision_pose_error_736);
    cp.velocity = raw(p.vectors_400_416[1]);
    cp.forward = t.travel_direction;
    cp.up = up;
    cp.ground_normal = {n.x, n.y, n.z, 0.0f};
    cp.time_step = p.timestep_2604;
    cp.mass = t.total_mass;
    const auto collision = skater.ground_runtime.calculate_collision_force(cp);
    auto& q = physics.board.forces_mut();
    if (collision) {
        q.append(QueuedPointForce{
            15,
            xyz(collision->force_2528),
            xyz(collision->point_2544),
        });
        // Original Slide caller discards the collision angular output.
    } else {
        q.append(point_force(4, front));
        q.append(point_force(5, rear));
        q.append(response.tag_16_force);
        q.append(sliding);
    }
    return ok();
}

rs::Result<float, Angle::Error> Angle::angle_between(std::array<float, 4> a, std::array<float, 4> b,
                                                     std::array<float, 4> c) {
    return sc::riding::collision_response::signed_angle(xyz(a), xyz(b), xyz(c));
}

QueuedPointForce point_force(std::uint32_t tag, std::array<float, 8> f) {
    return QueuedPointForce{
        tag,
        sc::math::Vector3::new_(f[0], f[1], f[2]),
        sc::math::Vector3::new_(f[4], f[5], f[6]),
    };
}

sc::math::Vector3 xyz(std::array<float, 4> v) { return sc::math::Vector3::new_(v[0], v[1], v[2]); }

std::array<float, 4> raw(std::array<std::uint32_t, 4> v) { return rs::from_bits(v); }

float fsel(float test, float a, float b) { return test >= -0.0f ? a : b; }

}  // namespace s3::game::physics::slide_state::update_
