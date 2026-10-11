// Port of skate-game/src/physics/air_phase/board.rs.
//!82D34AB0 consumes the completed Air reckoning's collision normal1216.
#include "s3/game/physics/air_phase/board.hpp"

#include "s3/core/math.hpp"
#include "s3/core/physics/force_queue.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::air_phase::board {

using ::s3::air::state::AirMath;
using ::s3::air::state::calculate_velocity_from_jump;
using ::s3::air::state::PhysicsAirFrame;
using ::s3::math::Vector3;
using ::s3::physics::force_queue::QueuedPointForce;
using ::s3::riding::collision_response::CollisionResponsePhysical;

Status advance(GamePhysics& physics, SkaterRuntime& skater, PhysicsAirFrame frame) {
    const auto& p = skater.player_input.processed;
    skater.ground.steering.update(0.0f, skater.air_settings.steering_blend, p.flags_2468, p.flags_2472);
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Air collision requires the actual BoardToolkit"));
    }
    const auto& t = *skater.player_input.toolkit;
    const auto normal = physics.riding.reckoning.ground_normal;
    CollisionResponsePhysical physical;
    physical.flags_2472 = p.flags_2472;
    physical.collision_displacement = rs::from_bits(p.collision_pose_error_736);
    physical.velocity = rs::from_bits(p.vectors_400_416[1]);
    physical.forward = t.travel_direction;
    physical.up = rs::from_bits(p.vectors_544_560_592_608[0]);
    physical.ground_normal = {normal.x, normal.y, normal.z, 0.0f};
    physical.time_step = p.timestep_2604;
    physical.mass = t.total_mass;
    if (const auto force = skater.ground_runtime.calculate_collision_force(physical)) {
        auto xyz = [](std::array<float, 4> v) { return Vector3::new_(v[0], v[1], v[2]); };
        physics.board.forces_mut().append(QueuedPointForce{
            15,
            xyz(force->force_2528),
            xyz(force->point_2544),
        });
    } else if (!skater.air_state.use_centre_of_mass_velocity && frame.frames_since_jump_correction_2576 < 12) {
        AirMath math;
        const auto velocity = calculate_velocity_from_jump(frame, frame.frames_since_jump_correction_2576, math);
        skater.ground_runtime.set_animated_velocity(physics.board, velocity);
    }
    return ok();
}

}  // namespace s3::game::physics::air_phase::board
