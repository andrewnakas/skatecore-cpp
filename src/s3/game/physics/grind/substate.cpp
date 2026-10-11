// Port of skate-game/src/physics/grind/substate.rs.
#include "s3/game/physics/grind/substate.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/grind_forces/launch.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::grind::substate {

using V = std::array<float, 4>;
using LaunchInput = ::s3::physics::grind_forces::launch::Input;
using ::s3::riding::collision_response::collision_response;
using ::s3::riding::collision_response::CollisionResponsePhysical;

Status execute(GamePhysics& physics, SkaterRuntime& skater, const ManagerObservation& manager) {
    S3G_TRY_OPT(const Family family, skater.grind.active, "Grind substate requires active physical family");
    const std::size_t idx = index(family);
    const auto state = skater.grind.states[idx];
    bool jumped_now = false;
    std::optional<V> prediction_velocity;
    S3G_TRY(const auto collision, collision_force(physics, skater));
    if (collision) {
        //82D40BC8 queues force15; optional velocity is prediction-only here.
        prediction_velocity = *collision;
    } else if ((skater.player_input.processed.flags_2468 & 0x00400000u) != 0) {
        const auto& p = skater.player_input.processed;
        if (!skater.player_input.toolkit) {
            return rs::make_err(std::string("Grind pop requires current BoardToolkit"));
        }
        const V position = skater.player_input.toolkit->deck[3];
        //Native virtual64 (vertical) precedes virtual60 (geometry side).
        S3G_TRY(float vertical_jump, skater.grind.settings.substate.vertical(family, p.state_variant_index_2528,
                                                                             skater.animation_input.extra.jump_strength));
        vertical_jump = vertical_jump * physics.trainer.grind_pop;
        const float geometry_side_jump = settings::side(family, p.grind.geometry_kind_1464);
        LaunchInput input;
        input.velocity_400 = rs::from_bits(p.vectors_400_416[0]);
        input.position_112 = position;
        input.current_point_1120 = rs::from_bits(p.grind.point_1120);
        input.balance_2800 = skater.animation_input.extra.grind_stability_nudge;
        input.geometry_side_jump = geometry_side_jump;
        input.vertical_jump = vertical_jump;
        const V velocity = launch::apply(physics.board, skater.player_input.grind.jumper,
                                         skater.player_input.processed.flags_2476, input);
        auto& s = skater.grind.states[idx];
        s.already_jumped = true;
        s.output.just_jumped = true;
        s.output.jump_velocity = velocity;
        jumped_now = true;
        prediction_velocity = velocity;
    } else if (!state.already_jumped) {
        if ((skater.player_input.processed.grind.flags_1516 & 0x40000000u) != 0) {
            const V velocity = rs::from_bits(skater.player_input.processed.grind.entry_velocity_1184);
            //82D40C84 SetVelocity updates all seven real board parts.
            skater.ground_runtime.set_animated_velocity(physics.board, velocity);
            prediction_velocity = velocity;
        } else {
            //Do not restore the copied State after these calls: each derived
            //implementation owns its full state writes, including transitions.
            switch (state.output.substate) {
                case 1: S3G_CHECK(contact::advance(physics, skater, manager)); break;
                case 2: S3G_CHECK(involuntary::advance(physics, skater, manager)); break;
                default: break;
            }
        }
    }
    if (prediction_velocity) {
        if (!skater.player_input.toolkit) {
            return rs::make_err(std::string("Grind prediction requires current BoardToolkit"));
        }
        const V position = skater.player_input.toolkit->deck[3];
        const float dt = skater.player_input.processed.timestep_2604;
        V prediction;
        for (std::size_t i = 0; i < 4; ++i) prediction[i] = rs::mul_add((*prediction_velocity)[i], dt, position[i]);
        //82D40CFC stores16112 and82D40D0C sets16416. No guessed trajectory.
        skater.animated_skeleton.roots.predicted_board_position = prediction;
        skater.animated_skeleton.roots.supplied_prediction = prediction;
    }
    if (jumped_now) {
        S3G_CHECK(skeleton::animated(physics, skater));
    } else {
        //82BDF530 then82C04368: CURRENT wrapper also retains physics error.
        S3G_CHECK(input_phase::update_ground(physics, skater));
    }
    return ok();
}

rs::Result<std::optional<V>, std::string> collision_force(GamePhysics& physics, const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Grind collision requires current BoardToolkit"));
    }
    const auto& toolkit = *skater.player_input.toolkit;
    const auto normal = physics.riding.reckoning.ground_normal;
    CollisionResponsePhysical physical;
    physical.flags_2472 = p.flags_2472;
    physical.collision_displacement = rs::from_bits(p.collision_pose_error_736);
    physical.velocity = rs::from_bits(p.vectors_400_416[1]);
    physical.forward = toolkit.travel_direction;
    physical.up = rs::from_bits(p.vectors_544_560_592_608[0]);
    physical.ground_normal = {normal.x, normal.y, normal.z, 0.0f};
    physical.time_step = p.timestep_2604;
    physical.mass = toolkit.total_mass;
    const auto response = collision_response(skater.grind.settings.substate.collision, physical);
    if (!response || !response->applied) {
        return std::optional<V>{};
    }
    auto xyz = [](V v) { return ::s3::math::Vector3::new_(v[0], v[1], v[2]); };
    //A full native queue drops this append but still takes the true branch.
    physics.board.forces_mut().append(::s3::physics::force_queue::QueuedPointForce{
        15, xyz(response->force), xyz(response->point)});
    return std::optional<V>{response->target_velocity};
}

}  // namespace s3::game::physics::grind::substate
