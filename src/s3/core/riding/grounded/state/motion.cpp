// Port of skate-core/src/riding/grounded/state/motion.rs.
#include "s3/core/riding/grounded/state/motion.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/riding/ground_correction_math.hpp"

namespace s3::riding::grounded::state::motion {

using ground_correction_math::dot_product;

namespace {

Vector3 xyz(const std::array<float, 4>& v) { return Vector3{v[0], v[1], v[2]}; }
std::array<float, 4> words(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
float reciprocal(float value) {
    float r = physics::native_arithmetic::reciprocal_estimate(value);
    for (int n = 0; n < 2; ++n) {
        r = rs::mul_add(r, rs::mul_add(-value, r, 1.0f), r);
    }
    return r;
}

}  // namespace

Vector3 entry_angular_velocity(std::array<float, 4> normal, std::array<float, 4> angular) {
    const float speed = dot_product(normal, angular);
    return xyz(rs::map(normal, [speed](float v) { return v * speed; }));
}

float entry_target_speed(Vector3 velocity, std::array<float, 4> normal, std::array<float, 4> forward) {
    const auto v = words(velocity);
    const float along_normal = dot_product(v, normal);
    std::array<float, 4> tangent{};
    for (std::size_t i = 0; i < 4; ++i) tangent[i] = v[i] - normal[i] * along_normal;
    const float raw = rs::abs(dot_product(v, forward));
    const float planar = rs::abs(dot_product(tangent, forward));
    return raw - planar >= 0.0f ? planar : raw;
}

QueuedPointForce landing_on_deck_force(std::array<float, 4> physical_velocity, std::array<float, 4> animation_velocity,
                                       std::array<float, 4> axis, float mass, float strength, float point_y) {
    std::array<float, 4> delta{};
    for (std::size_t i = 0; i < 4; ++i) delta[i] = animation_velocity[i] - physical_velocity[i];
    const float projected = dot_product(delta, axis);
    const float step_inverse = reciprocal(rs::from_bits(0x3c888889u));
    std::array<float, 4> force{};
    for (std::size_t i = 0; i < 4; ++i) {
        force[i] = ((delta[i] - axis[i] * projected) * strength) * mass * step_inverse;
    }
    return QueuedPointForce{19, xyz(force), Vector3{0.0f, point_y, 0.0f}};
}

std::optional<Vector3> future_deck_displacement(const BoardForceQueue& forces, float mass, float dt,
                                                std::array<float, 4> normal, bool pushing, bool manual_correction) {
    if (pushing || manual_correction) {
        return std::nullopt;
    }
    std::array<float, 4> sum{0.0f, 0.0f, 0.0f, 0.0f};
    for (const auto& force : forces.entries()) {
        const auto v = words(force.force_world);
        for (std::size_t i = 0; i < 4; ++i) sum[i] += v[i];
    }
    const float scale = reciprocal(mass) * dt;
    const auto delta = rs::map(sum, [scale](float v) { return v * scale; });
    const float projection = dot_product(delta, normal);
    std::array<float, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = (delta[i] - normal[i] * projection) * dt;
    return xyz(out);
}

}  // namespace s3::riding::grounded::state::motion
