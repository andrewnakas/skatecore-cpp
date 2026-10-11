// Port of skate-core/src/physics/solver/contact_build/input.rs.
// Decodes the uncompiled contact copied by the native contact producer.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::solver::contact_build::input {

using math::Vector3;

struct BodyInput {
    Vector3 center;
    Vector3 inverse_inertia_full;
    Vector3 inverse_inertia_split;
    float inverse_mass = 0.0f;
    std::uint32_t state = 0;
    Vector3 force_acceleration;
    Vector3 torque_acceleration;
    std::uint32_t reaction_id = 0;
};

struct ContactInput {
    std::array<Vector3, 2> positions{};
    std::array<Vector3, 3> axes{};
    Vector3 relative_velocity;
    float restitution = 0.0f;
    std::array<BodyInput, 2> bodies{};
    std::array<std::uint32_t, 2> body_ids{};
    std::uint32_t static_friction_bits = 0;
    std::uint32_t dynamic_friction_bits = 0;
    std::uint32_t contact_tag = 0;

    static ContactInput decode(const std::array<std::uint32_t, 64>& words) {
        auto vector = [&](std::size_t row) {
            return Vector3{rs::from_bits(words[row * 4]), rs::from_bits(words[row * 4 + 1]),
                           rs::from_bits(words[row * 4 + 2])};
        };
        ContactInput in;
        in.positions = {vector(0), vector(1)};
        for (std::size_t axis = 0; axis < 3; ++axis) in.axes[axis] = vector(2 + axis);
        in.relative_velocity = vector(5);
        in.restitution = rs::from_bits(words[11]);
        for (std::size_t body = 0; body < 2; ++body) {
            in.bodies[body] = BodyInput{
                vector(6 + body),
                vector(8 + body),
                vector(10 + body),
                rs::from_bits(words[(8 + body) * 4 + 3]),
                words[(10 + body) * 4 + 3],
                vector(12 + body),
                vector(14 + body),
                words[(6 + body) * 4 + 3],
            };
        }
        in.body_ids = {words[3], words[7]};
        in.static_friction_bits = words[15];
        in.dynamic_friction_bits = words[19];
        in.contact_tag = words[23];
        return in;
    }
};

}  // namespace s3::physics::solver::contact_build::input
