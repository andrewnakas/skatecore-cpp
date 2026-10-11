// Port of skate-game/src/physics/ground_phase.rs.
//! Production Ground entry82D37538 and update82D37C88/82D38800.
//! State selection belongs to the player coordinator. This phase consumes its
//! current processed packet and changes the same board that the solver advances.
#include "s3/game/physics/ground_phase.hpp"

#include "s3/core/physics/ground_hang_geometry.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_contact_response.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/air_phase.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/handplant.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/physics/solve.hpp"

namespace s3::game::physics::ground_phase {

using ground_runtime::GroundEntryTargets;
using ground_runtime::GroundInputObservations;
using ground_runtime::GroundLaunchInfo;
using ground_runtime::GroundLaunchPhysical;
using ground_runtime::GroundPhysicalFrame;
using ground_runtime::GroundUpdateFrame;
using ground_runtime::GroundUpdateTargets;

namespace {
std::array<float, 4> raw(std::array<std::uint32_t, 4> v) { return rs::from_bits(v); }
std::array<float, 4> lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
Vector3 xyz(std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); }
}  // namespace

GroundLifecycle GroundLifecycle::new_() {
    GroundLifecycle l;
    l.skeleton_controller = skeleton_controller::SkeletonControllerState::new_();
    l.skeleton_elapsed_16505 = false;
    l.board_animated_290 = 0;
    l.manual_drag_2724 = 0.0f;
    l.edge = std::nullopt;
    l.pending_wall_jump = std::nullopt;
    return l;
}

void reset_board_state(ground_runtime::state::GroundState& ground, ground_runtime::GroundRuntime& runtime,
                       GroundLifecycle& life, bool& board_wiping_out) {
    ground.steering.deck_tilt = 0.0f;               //82C06048, wrapper256.
    ground.steering.targets = {0.0f, 0.0f};         //82C06060/64, body7680/7684.
    runtime.reset_board_toolkit();
    life.board_animated_290 = 0;                    //82C06070.
    board_wiping_out = false;                       //82C0D79C..7A0 clears body8384 high2bits.
}

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Ground entry requires PlayerInput's current board toolkit"));
    }
    const auto& toolkit = *skater.player_input.toolkit;
    //82D37560: full SetStandard, after SetPhysicsState's possession Stop and
    //old-state Exit, before Ground disables the animation drives. Use retained
    //stock materials and publish the actual collider flags, not reset defaults.
    {
        auto effects = skater.board_possession_live.effects(physics, skater.ground_lifecycle.board_animated_290,
                                                            skater.player_input.processed.timestep_2604);
        effects.standard_board();
    }
    skater.board_possession_live.publish_volumes(physics);
    return enter_components(physics.board, skater.player_input.processed, skater.animation_input, toolkit,
                            skater.ground, skater.ground_lifecycle, skater.air_reckoning.state, skater.wipeout.state,
                            skater.skeleton_collision, skater.foot_ik, skater.skeleton_output,
                            physics.board_wiping_out);
}

Status enter_components(sc::physics::board_runtime::BoardRuntime& board,
                        const sc::player::input_phase::ProcessedPhysicsInput& processed,
                        const animation_input::AnimationInput& animation,
                        const sc::physics::board_toolkit::BoardToolkit& toolkit,
                        ground_runtime::state::GroundState& ground, GroundLifecycle& life,
                        sc::air::reckoning::AirState& air_reckoning, sc::player::wipeout::Requests& wipeout,
                        sc::physics::skeleton_body::SkeletonCollisionMode& collision_mode, foot_ik::FootIk& ik,
                        skeleton_output::SkeletonOutput& output, bool& board_wiping_out) {
    // GamePhysics owns the meaningful high bit of Skateboard8384.
    std::uint8_t board_flags = static_cast<std::uint8_t>(std::uint8_t(board_wiping_out) << 7);
    auto collision = [&](std::uint32_t state) -> Status {
        if (state != 6) {
            return rs::make_err(std::string("Ground requested a non-ground skeleton collision state"));
        }
        return life.skeleton_controller.request_ground(collision_mode);
    };
    auto landing = [&](bool reverse) -> Status {
        output.trigger_wobble(true, reverse);
        return ok();
    };
    const auto result = ground.enter(board, processed, animation, toolkit,
                                     GroundEntryTargets{
                                         ik,
                                         life.skeleton_elapsed_16505,
                                         air_reckoning.spin_angle,
                                         air_reckoning.spin_speed,
                                         board_flags,
                                         life.board_animated_290,
                                         wipeout.mode,
                                         wipeout.balance,
                                         collision,
                                         landing,
                                     });
    board_wiping_out = (board_flags & 0x80u) != 0;
    return result;
}

rs::Result<sc::riding::grounded::state::board::GroundBoardOutcome, std::string> advance(GamePhysics& physics,
                                                                                         SkaterRuntime& skater) {
    handplant::ground_query(physics, skater);
    S3G_TRY(const auto selector_input, air_phase::selector_input(physics, skater));
    const auto& p = skater.player_input.processed;
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Ground update requires PlayerInput's current board toolkit"));
    }
    const auto& toolkit = *skater.player_input.toolkit;
    auto& life = skater.ground_lifecycle;
    if (life.pending_wall_jump) {
        return rs::make_err(std::string("Ground wall jump is pending its trajectory selector continuation"));
    }
    // Native grind reset clears the complete observation block. An authored
    // world with no edges therefore has no active edge data to publish.
    const GroundEdge edge = life.edge.value_or(GroundEdge{
        0,
        {0.0f, 0.0f, 0.0f, 0.0f},
        Vector3::ZERO,
        Vector3::ZERO,
    });
    const auto normal = raw(p.vectors_464_480_496_512_528[0]);
    const auto up = raw(p.vectors_544_560_592_608[0]);
    const auto initial_velocity = raw(p.vectors_400_416[0]);
    auto velocity = initial_velocity;
    auto predicted = skater.animated_skeleton.roots.predicted_board_position;
    auto& targets = skater.skeleton_drives.targets;
    auto move_future = [&](Vector3 delta) -> Status {
        targets.apply_future_deck_displacement(delta);
        predicted[0] += delta.x;
        predicted[1] += delta.y;
        predicted[2] += delta.z;
        return ok();
    };
    const auto grind_context =
        air_trajectory::GrindContext::from_processed(skater.player_input.processed, solve::deck_frame(physics.board)[3]);
    auto launch = [&](const GroundLaunchInfo& info) -> Status {
        auto input = selector_input;
        input.board_vertical_velocity = info.velocity[1];
        S3G_CHECK(skater.trajectory.launch(info.selector_launch(), input, physics.world));
        S3G_CHECK(skater.trajectory.update(input, physics.world, grind_context));
        return ok();
    };
    sc::riding::ground_contact_response::WallRidePhysical wall_ride;
    wall_ride.board_normal = normal;
    wall_ride.up = up;
    wall_ride.velocity = velocity;
    wall_ride.board_mass = toolkit.total_mass;
    wall_ride.gravity = p.gravity_2648;
    wall_ride.speed = p.scalar_2652;
    wall_ride.contact_count = static_cast<std::int32_t>(p.wheel_count_2556);
    sc::riding::collision_response::CollisionResponsePhysical collision;
    collision.flags_2472 = p.flags_2472;
    collision.collision_displacement = raw(p.collision_pose_error_736);
    collision.velocity = raw(p.vectors_400_416[1]);
    collision.forward = toolkit.travel_direction;
    collision.up = up;
    collision.ground_normal = lanes(physics.riding.reckoning.ground_normal);
    collision.time_step = p.timestep_2604;
    collision.mass = toolkit.total_mass;
    const sc::physics::ground_hang_geometry::HangGeometryInput hang_geometry{edge.start, edge.end, xyz(edge.point)};
    GroundPhysicalFrame physical{
        skater.animated_skeleton.record,
        wall_ride,
        collision,
        hang_geometry,
        velocity,
        physics.settings.wheel_material,
        skater.wipeout.state,
        p.timestep_2604,
        GroundLaunchPhysical{
            physics.riding.reckoning_frames,
            skater.animated_skeleton.board_frames.local_centre_of_mass,
            skater.animated_skeleton.board_frames.local_board_position,
            toolkit.deck[3],
            raw(p.vectors_544_560_592_608[2]),
            initial_velocity,
            raw(p.prepared_jump_704),
            p.flags_2468,
            p.timestep_2604,
        },
        launch,
    };
    GroundInputObservations extra;
    extra.manual_drag_2724 = life.manual_drag_2724;
    extra.trajectory_state_bits = p.external_physics_1616.flags;
    extra.edge_flags = edge.flags;
    extra.edge_point = edge.point;
    const auto settings = skater.ground_settings;  // Arc clone keeps the profile alive
    auto result = skater.ground.update(
        skater.ground_runtime, physics.board, physics.world, *settings,
        GroundUpdateFrame{
            p,
            skater.animation_input,
            physics.riding,
            skater.animated_skeleton,
            toolkit,
            physics.settings.step.base_truck_transforms,
            extra,
        },
        std::move(physical),
        GroundUpdateTargets{
            skater.foot_ik,
            life.skeleton_elapsed_16505,
            skater.skeleton_output.correction.pending,
            // Original830BD4A0 initializer82F825F0 splats8216DEE0=-1.
            move_future,
            skater.offboard_grab,
        });
    // These source writes occur during update. Publish even on a later branch
    // error so retrying cannot silently erase a completed physical side effect.
    skater.player_input.processed.vectors_400_416[0] = rs::to_bits(velocity);
    skater.animated_skeleton.roots.predicted_board_position = predicted;
    return result;
}

}  // namespace s3::game::physics::ground_phase
