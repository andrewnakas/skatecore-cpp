// Port of skate-core/src/physics/grind_forces/launch.rs.
#include "s3/core/physics/grind_forces/launch.hpp"

namespace s3::physics::grind_forces::launch {

namespace {
V cross(const V& a, const V& b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
}  // namespace

Output launch(Jumper jumper, Input input) {
    const auto geometry = jumper.geometry;
    V normal = geometry.normal;
    V candidate{};
    if (jumper.family == 2 || jumper.family == 4) {
        const V across = cross(normal, geometry.direction);
        const V outward =
            dot3(across, sub(input.position_112, geometry.point)) > 0.0f ? across : scale(across, -1.0f);
        const float amount = jumper.family == 4 ? -0.1093f : 0.291f;
        for (std::size_t i = 0; i < 4; ++i) candidate[i] = normal[i] + outward[i] * amount;
    } else {
        for (std::size_t i = 0; i < 4; ++i) {
            candidate[i] = rs::mul_add(geometry.upmost[i], 0.5f, normal[i] * 0.5f);
        }
    }
    const float magnitude = length(candidate);
    if (magnitude > 0.1f) {
        normal = scale(candidate, reciprocal(magnitude));
    }
    const V planar = sub(input.velocity_400, scale(normal, dot3(input.velocity_400, normal)));
    const float vertical = input.vertical_jump * 1.03f;
    V base{};
    for (std::size_t i = 0; i < 4; ++i) base[i] = rs::mul_add(normal[i], vertical, planar[i]);
    const V across = cross(geometry.direction, normal);
    float balance = input.balance_2800;
    const bool low_energy = jumper.energy < 0.31f;
    if (low_energy) {
        if (balance < 0.0f) {
            if (balance - -0.42f >= 0.0f) {
                balance = -0.42f;
            }
        } else if (balance > 0.0f) {
            if (!(balance - 0.42f >= 0.0f)) {
                balance = 0.42f;
            }
        } else {
            balance = dot3(sub(input.position_112, input.current_point_1120), across) > 0.0f ? 0.42f : -0.42f;
        }
        jumper.cooldown = 10;
    }
    const float side = geometry.geometry_kind != 0 ? input.geometry_side_jump : 0.0f;
    V lateral = sub(scale(across, balance * 1.8f), scale(geometry.high_side, side));
    const float lateral_magnitude = length(lateral);
    if (lateral_magnitude > 1.8f) {
        lateral = scale(lateral, 1.8f / lateral_magnitude);
    }
    V velocity{};
    for (std::size_t i = 0; i < 4; ++i) velocity[i] = base[i] + lateral[i];
    jumper.launched = true;
    const float energy = jumper.energy - 0.6f;
    jumper.energy = -energy >= 0.0f ? 0.0f : energy;
    return Output{velocity, jumper, low_energy ? 0x02000000u : 0u};
}

}  // namespace s3::physics::grind_forces::launch
