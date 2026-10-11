// Port of skate-core/src/player/wipeout_state/torque.rs (whole file).
#include "s3/core/player/wipeout_state/torque.hpp"

namespace s3::player::wipeout_state::torque {

V angular_velocity(const physics::skeleton_body::SkeletonPhysicalRecord& record,
                   const std::array<float, 24>& fractional) {
    V result{};
    for (std::size_t part = 1; part < 24; ++part) {
        const V radial = math::sub(record.positions[part], record.centre_of_mass);
        const V velocity = math::sub(record.velocities[part], record.centre_of_mass_velocity);
        const float squared = math::dot(radial, radial);
        if (squared > rs::from_bits(0x38D1B717u)) {
            const V term = math::scale(math::cross(radial, velocity), math::reciprocal(squared));
            result = math::madd(term, fractional[part], result);
        }
    }
    return result;
}

void apply(physics::skeleton_body::SkeletonBody& body, V physical_com, V control) {
    const float magnitude = math::length(control);
    for (std::size_t part = 1; part < 24; ++part) {
        const V radial = math::sub(body.record.pose[part][3], physical_com);
        if (math::dot(radial, radial) > 0.001f) {
            const V tangent = math::normalize_or(math::cross(control, radial), V{});
            body.apply_part_displacement(part, math::scale(tangent, magnitude));
        }
    }
}

}  // namespace s3::player::wipeout_state::torque
