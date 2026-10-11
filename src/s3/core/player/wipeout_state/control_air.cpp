// Port of skate-core/src/player/wipeout_state/control_air.rs (whole file).
#include "s3/core/player/wipeout_state/control_air.hpp"

#include "s3/core/player/wipeout_state/torque.hpp"

namespace s3::player::wipeout_state::control_air {

using math::V;

bool update(State& state, physics::skeleton_body::SkeletonBody& body, V physical_com, std::array<float, 2> input,
            bool contact) {
    const V UP{0.0f, 1.0f, 0.0f, 0.0f};
    const V spine = math::normalize_or(math::sub(body.record.pose[11][3], body.record.pose[23][3]), V{});
    state.angular_velocity = torque::angular_velocity(body.record, body.definition.animation_masses.fractional);
    if (contact) {
        return false;
    }
    const V direction = math::normalize_or(state.velocity, V{});
    V sideways = math::normalize_or(math::cross(UP, spine), V{});
    sideways[1] = 0.0f;
    sideways = math::normalize_or(sideways, V{});
    const V velocity_side = math::cross(UP, direction);
    if (math::dot(velocity_side, sideways) < 0.0f) {
        sideways = math::scale(sideways, -1.0f);
    }
    const V plane_normal = math::normalize_or(velocity_side, V{});
    V projected = math::sub(spine, math::scale(plane_normal, math::dot(plane_normal, spine)));
    projected = math::normalize_or(projected, V{});
    V automatic = math::scale(math::cross(spine, projected), 0.5f);
    const V hips_up = body.record.pose[23][1];
    if (rs::abs(input[0]) < 0.5f && hips_up[1] < 0.0f) {
        V p = math::sub(hips_up, math::scale(plane_normal, math::dot(plane_normal, hips_up)));
        p = math::normalize_or(p, V{});
        automatic = math::madd(math::cross(hips_up, p), 0.5f, automatic);
    }
    // Both original scalar limits820993E8/EC are8 degrees.
    const float limit = 8.0f * rs::from_bits(0x3C8EFA35u);
    const V side = math::scale(sideways, limit * input[1]);
    const V negative_spine = rs::map(spine, [](float v) { return rs::from_bits(rs::to_bits(v) ^ 0x80000000u); });
    V target = math::add(math::madd(negative_spine, limit * input[0], side), automatic);
    target = math::scale(target, rs::from_bits(0x426FFFFFu));
    const V delta = math::scale(math::scale(math::sub(target, state.angular_velocity), rs::from_bits(0x4019999Au)),
                                rs::from_bits(0x3C888889u));
    torque::apply(body, physical_com, math::limit_length(delta, 0.5f));
    return true;
}

}  // namespace s3::player::wipeout_state::control_air
