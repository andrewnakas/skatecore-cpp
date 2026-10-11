// Port of skate-core/src/riding/ground_contact_response.rs.
#include "s3/core/riding/ground_contact_response.hpp"

#include "s3/core/physics/board_motion_output.hpp"

namespace s3::riding::ground_contact_response {

using math::Vector3;
using physics::board_motion_output::dot;
using physics::force_queue::QueuedPointForce;

namespace {
Vector3 xyz(const std::array<float, 4>& v) { return Vector3{v[0], v[1], v[2]}; }
}  // namespace

GroundContactResponse wall_ride_response(const WallRideSettings& settings, WallRidePhysical physical,
                                         GroundContactFrame frame, std::array<float, 4> previous_velocity) {
    GroundContactResponse response;
    response.active_2731 = false;
    response.tag_16_force = QueuedPointForce{16, Vector3{}, Vector3{}};
    response.vector_2688 = previous_velocity;
    response.scalar_2704 = 0.0f;
    response.animated_board_2708 = false;
    // 82D94C10 installs literal .5 and .71 in the two first gates.
    if (!frame.flag_8084 || !(rs::abs(physical.board_normal[1]) < 0.5f) ||
        !(dot(xyz(physical.up), xyz(physical.board_normal)) > 0.71f)) {
        return response;
    }
    response.active_2731 = true;
    const float acceleration = settings.anti_gravity_vs_time.evaluate(frame.scalar_2752);
    response.tag_16_force.force_world =
        Vector3{0.0f, -((acceleration * physical.board_mass) * physical.gravity), 0.0f};
    const float time = frame.scalar_2756;
    const bool consider = (time < 0.0f && time > -1.0f) || (physical.speed < 3.0f && time > 0.1f);
    if (!(consider || (frame.scalar_2752 > 0.01f && physical.contact_count > 0) ||
          frame.scalar_2752 > settings.foot_force_time)) {
        return response;
    }
    // The dot is loaded BEFORE the temporary slot is overwritten by the
    // height/velocity expression at82D94044. Older decompiles alias the two.
    const float floor_wall = dot(xyz(frame.vector_8064), xyz(physical.board_normal));
    const float height = rs::abs(frame.vector_8048[1] - frame.vector_8032[1]);
    const float predicted_height = rs::mul_add(physical.velocity[1], settings.velocity_time_to_consider, height);
    if (!(floor_wall < settings.max_dot_floor_wall &&
          (consider || predicted_height < settings.auto_jump_height || time > settings.max_time))) {
        return response;
    }
    response.animated_board_2708 = true;
    const float into_normal = dot(xyz(physical.board_normal), xyz(physical.velocity));
    std::array<float, 4> tangent{};
    for (std::size_t i = 0; i < 4; ++i) tangent[i] = physical.velocity[i] - (physical.board_normal[i] * into_normal);
    if (tangent[1] < 0.0f && tangent[1] > -6.0f) {
        tangent[1] *= settings.auto_jump_y_down_scalar;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        response.vector_2688[i] = rs::mul_add(physical.board_normal[i], settings.auto_jump_force, tangent[i]);
    }
    return response;
}

}  // namespace s3::riding::ground_contact_response
