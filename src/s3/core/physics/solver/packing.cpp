// Port of skate-core/src/physics/solver/packing.rs.
#include "s3/core/physics/solver/packing.hpp"

namespace s3::physics::solver::packing {

using math::Vector3;
using rigid_body::RetailPackedWorldInverseInertia;

namespace {
template <std::size_t N>
void row(std::array<std::uint32_t, N>& words, std::size_t i, std::array<float, 3> v, float w) {
    words[4 * i] = rs::to_bits(v[0]);
    words[4 * i + 1] = rs::to_bits(v[1]);
    words[4 * i + 2] = rs::to_bits(v[2]);
    words[4 * i + 3] = rs::to_bits(w);
}
std::array<float, 3> xyz(Vector3 v) { return {v.x, v.y, v.z}; }
template <std::size_t N>
void vector(std::array<std::uint32_t, N>& words, std::size_t i, Vector3 v, float w) {
    row(words, i, xyz(v), w);
}
std::array<Vector3, 3> inertia_columns(RetailPackedWorldInverseInertia i) {
    return {
        Vector3{i.full.x, i.full.y, i.full.z},
        Vector3{i.full.y, i.split.y, i.split.z},
        Vector3{i.full.z, i.split.z, i.split.x},
    };
}
}  // namespace

packed::Contact contact(const RetailContactJacobian& c) {
    return packed::Contact{c.words, c.reaction_index_a, c.reaction_index_b};
}

packed::Drive drive(const RetailDriveRows& d) {
    std::array<std::uint32_t, 96> words{};
    vector(words, 0, d.arm_a, 0.0f);
    vector(words, 1, d.arm_b, 0.0f);
    row(words, 2, d.accumulated_linear_impulse, d.linear_softness);
    row(words, 3, d.accumulated_angular_impulse, d.angular_softness);
    for (std::size_t component = 0; component < 3; ++component) {
        std::array<float, 3> linear{};
        std::array<float, 3> angular{};
        for (std::size_t r = 0; r < 3; ++r) {
            linear[r] = xyz(d.linear_axes[r])[component] * d.linear_inverse_effective_mass[r];
        }
        row(words, 4 + component * 2, linear, 0.0f);
        for (std::size_t r = 0; r < 3; ++r) {
            angular[r] = xyz(d.angular_axes[r])[component] * d.angular_inverse_effective_mass[r];
        }
        row(words, 5 + component * 2, angular, 0.0f);
        vector(words, 12 + component, d.linear_axes[component], 0.0f);
        vector(words, 15 + component, d.angular_axes[component], 0.0f);
    }
    row(words, 10, d.linear_target_impulse, d.linear_maximum_impulse[2]);
    row(words, 11, d.angular_target_impulse, d.angular_maximum_impulse[2]);
    words[6 * 4 + 3] = rs::to_bits(d.linear_maximum_impulse[0]);
    words[7 * 4 + 3] = rs::to_bits(d.linear_maximum_impulse[1]);
    words[8 * 4 + 3] = rs::to_bits(d.angular_maximum_impulse[0]);
    words[9 * 4 + 3] = rs::to_bits(d.angular_maximum_impulse[1]);
    const std::pair<std::size_t, const drive_solver::RetailDriveBodyState*> bodies[2] = {
        {18, &d.frame_a_body}, {21, &d.frame_b_body}};
    for (const auto& [index, body] : bodies) {
        if ((body->state & 4u) != 0) {
            const auto columns = inertia_columns(body->world_inverse_inertia);
            for (std::size_t i = 0; i < 3; ++i) {
                vector(words, index + i, columns[i], i == 0 ? body->inverse_mass : 0.0f);
            }
        }
    }
    return packed::Drive{words, d.frame_a_body.reaction_index, d.frame_b_body.reaction_index};
}

packed::Reaction reaction(RetailReactionCorrections r) {
    packed::Reaction words{};
    const Vector3 values[4] = {r.linear_displacement, r.position_displacement, r.angular_displacement,
                               r.orientation_displacement};
    for (std::size_t i = 0; i < 4; ++i) vector(words, i, values[i], 0.0f);
    return words;
}

RetailReactionCorrections unpack_reaction(const packed::Reaction& words) {
    auto v = [&](std::size_t i) {
        const auto x = read_xyz(words.data(), i);
        return Vector3{x[0], x[1], x[2]};
    };
    return RetailReactionCorrections{v(0), v(1), v(2), v(3)};
}

std::array<float, 3> read_xyz(const std::uint32_t* words, std::size_t i) {
    return {rs::from_bits(words[4 * i]), rs::from_bits(words[4 * i + 1]), rs::from_bits(words[4 * i + 2])};
}

}  // namespace s3::physics::solver::packing
