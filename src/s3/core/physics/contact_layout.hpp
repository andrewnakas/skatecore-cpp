// Port of skate-core/src/physics/contact_layout.rs.
// Serialization of the uncompiled TU3 256-byte contact, in guest word order.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/contact.hpp"

namespace s3::physics::contact_layout {

inline std::array<std::uint32_t, 4> lanes(math::Vector3 v, std::uint32_t w) {
    return {rs::to_bits(v.x), rs::to_bits(v.y), rs::to_bits(v.z), w};
}

inline std::array<std::array<std::uint32_t, 4>, 5> workspace(const contact::RetailContactWorkspace& w) {
    return {
        lanes(w.center_of_mass, w.reaction_id),
        lanes(w.inverse_inertia_full, rs::to_bits(w.inverse_mass)),
        lanes(w.inverse_inertia_split, w.state),
        lanes(w.force_acceleration, rs::to_bits(w.kinetic_energy)),
        lanes(w.torque_acceleration, w.cool_down),
    };
}

/// pub(super)
inline std::array<std::uint32_t, 64> encode(const contact::RetailContact& c) {
    std::array<std::uint32_t, 64> words{};
    const std::array<std::array<std::uint32_t, 4>, 6> header{
        lanes(c.position_on_a, c.body_a_id),
        lanes(c.position_on_b, c.body_b_id),
        lanes(c.normal, rs::to_bits(c.restitution)),
        lanes(c.tangent_0, rs::to_bits(c.static_friction)),
        lanes(c.tangent_1, rs::to_bits(c.dynamic_friction)),
        lanes(c.relative_velocity, c.tag),
    };
    for (std::size_t i = 0; i < header.size(); ++i) {
        for (std::size_t j = 0; j < 4; ++j) words[i * 4 + j] = header[i][j];
    }
    const auto a = workspace(c.body_a_workspace);
    const auto b = workspace(c.body_b_workspace);
    for (std::size_t i = 0; i < 5; ++i) {
        for (std::size_t j = 0; j < 4; ++j) {
            words[24 + i * 8 + j] = a[i][j];
            words[28 + i * 8 + j] = b[i][j];
        }
    }
    return words;
}

}  // namespace s3::physics::contact_layout
