// Port of skate-game/src/physics/ground_runtime/mod.rs.
#include "s3/game/physics/ground_runtime.hpp"

#include "s3/core/physics/deck_angular_correction.hpp"

namespace s3::game::physics::ground_runtime {

using sc::physics::board::BodyId;
using sc::physics::board::index;

void GroundRuntime::reset_board_toolkit() { retained_board_normal = {0.0f, 1.0f, 0.0f, 0.0f}; }

rs::Result<GroundRuntime, std::string> GroundRuntime::load(const sd::Collections& data) {
    auto feet = [&](const char* field) { return data.float_("physics_feet", "default", field); };
    auto collision = [&](const char* field) { return data.float_("physics_collision", "default", field); };
    GroundRuntime out;
    //SkateboardReset82C05F50 explicitly clears192..208, then sets196=1.
    out.retained_board_normal = {0.0f, 1.0f, 0.0f, 0.0f};
    S3G_TRY(out.launch_cone_x, data.float_("physics_trajectory", "default", "ConeAngleX"));
    S3G_TRY(out.launch_cone_z, data.float_("physics_trajectory", "default", "ConeAngleZ"));
    S3G_TRY(out.deck_center_to_truck, data.float_("physics_grinds", "default", "DeckCenterToTruck"));
    S3G_TRY(out.wall_ride.anti_gravity_vs_time, curve(data, "physics_feet", "WallRideAntiGravityVsTime"));
    S3G_TRY(out.wall_ride.max_dot_floor_wall, feet("WallRideMaxDotFloorWall"));
    S3G_TRY(out.wall_ride.foot_force_time, feet("WallRideFootForceTime"));
    S3G_TRY(out.wall_ride.auto_jump_height, feet("WallRideAutoJumpHeight"));
    S3G_TRY(out.wall_ride.max_time, feet("WallRideMaxTime"));
    S3G_TRY(out.wall_ride.velocity_time_to_consider, feet("WallRideVelTimeToConsider"));
    S3G_TRY(out.wall_ride.auto_jump_y_down_scalar, feet("WallRideAutoJumpYDownScalar"));
    S3G_TRY(out.wall_ride.auto_jump_force, feet("WallRideAutoJumpForce"));
    S3G_TRY(out.collision.maximum_velocity_delta, collision("MaxVelDelta"));
    S3G_TRY(out.collision.force_y_offset, collision("ForceYOffset"));
    S3G_TRY(out.collision.force_scalar, collision("CollisionForceScalar"));
    S3G_TRY(out.collision.target_displacement_velocity, collision("TargetDisplacementVel"));
    S3G_TRY(out.collision.torque_vs_angle, curve(data, "physics_collision", "CollisionTorqueVsAngle"));
    //PhysicsGround ctor/reset initializes this output vector tozero.
    out.contact = GroundContactResponse{};
    out.contact.active_2731 = false;
    out.contact.tag_16_force = sc::physics::force_queue::QueuedPointForce{};
    out.contact.tag_16_force.tag = 16;
    out.contact.vector_2688 = {0.0f, 0.0f, 0.0f, 0.0f};
    out.contact.scalar_2704 = 0.0f;
    out.contact.animated_board_2708 = false;
    out.collision_force = std::nullopt;
    return out;
}

GroundContactResponse GroundRuntime::contact_response_with_previous(
    GroundContactFrame frame, sc::riding::ground_contact_response::WallRidePhysical physical,
    std::array<float, 4> previous) const {
    return sc::riding::ground_contact_response::wall_ride_response(wall_ride, physical, frame, previous);
}

void GroundRuntime::update_body_accumulator(sc::physics::board_runtime::BoardRuntime& board) {
    sc::physics::deck_angular_correction::apply_ground_body_torque(board.bodies_mut()[index(BodyId::Deck)].rates);
}

std::optional<CollisionForceResponse> GroundRuntime::calculate_collision_force(
    sc::riding::collision_response::CollisionResponsePhysical physical) {
    auto result = sc::riding::collision_response::collision_response(collision, physical);
    if (!result) return std::nullopt;
    if (!result->applied) {
        return std::nullopt;
    }
    CollisionForceResponse output{result->force, result->point, result->angular_displacement};
    collision_force = output;
    return output;
}

void GroundRuntime::apply_angular_target(sc::physics::board_runtime::BoardRuntime& board,
                                         std::array<float, 4> displacement) {
    sc::physics::deck_angular_correction::apply_limited_displacement(board.bodies_mut()[index(BodyId::Deck)].rates,
                                                                     xyz(displacement));
}

void GroundRuntime::apply_angular_displacement(sc::physics::board_runtime::BoardRuntime& board,
                                               std::array<float, 4> displacement) {
    sc::physics::deck_angular_correction::apply_angular_displacement(board.bodies_mut()[index(BodyId::Deck)].rates,
                                                                     xyz(displacement));
}

void GroundRuntime::set_animated_velocity(sc::physics::board_runtime::BoardRuntime& board,
                                          std::array<float, 4> velocity) {
    for (auto& body : board.bodies_mut()) {
        body.rates.linear_velocity = xyz(velocity);
    }
}

rs::Result<sc::point_graph::PointGraph<8>, std::string> curve(const sd::Collections& data, const char* class_,
                                                              const char* field) {
    S3G_TRY(auto words, data.words<20>(class_, "default", field));
    sc::point_graph::PointGraph<8> out{};
    for (std::size_t i = 0; i < 8; ++i) out.x[i] = rs::from_bits(words[4 + i]);
    for (std::size_t i = 0; i < 8; ++i) out.y[i] = rs::from_bits(words[12 + i]);
    return out;
}

sc::math::Vector3 xyz(std::array<float, 4> v) { return sc::math::Vector3::new_(v[0], v[1], v[2]); }

}  // namespace s3::game::physics::ground_runtime
