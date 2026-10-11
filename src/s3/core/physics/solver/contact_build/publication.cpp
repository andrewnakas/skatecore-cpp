// Port of skate-core/src/physics/solver/contact_build/publication.rs.
#include "s3/core/physics/solver/contact_build.hpp"

namespace s3::physics::solver::contact_build::publication {

namespace {
void vector_row(std::array<std::uint32_t, 64>& words, std::size_t row, Vector3 xyz, std::uint32_t carry) {
    words[row * 4] = rs::to_bits(xyz.x);
    words[row * 4 + 1] = rs::to_bits(xyz.y);
    words[row * 4 + 2] = rs::to_bits(xyz.z);
    words[row * 4 + 3] = carry;
}

void float_row(std::array<std::uint32_t, 64>& words, std::size_t row, std::array<float, 4> values) {
    for (std::size_t i = 0; i < 4; ++i) words[row * 4 + i] = rs::to_bits(values[i]);
}
}  // namespace

std::array<std::uint32_t, 64> compile(const ContactPreparation& prepared, std::array<float, 3> inverse_response) {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t body = 0; body < 2; ++body) {
        vector_row(words, body, prepared.arms[body], prepared.reaction_ids[body]);
    }
    std::array<std::array<float, 3>, 3> axes{};
    for (std::size_t i = 0; i < 3; ++i) axes[i] = {prepared.axes[i].x, prepared.axes[i].y, prepared.axes[i].z};
    const std::array<std::uint32_t, 3> carry{
        prepared.combined_state_bit_8,
        prepared.static_friction_bits,
        prepared.dynamic_friction_bits,
    };
    for (std::size_t component = 0; component < 3; ++component) {
        std::array<float, 3> column{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            column[axis] = rs::mul_add(axes[axis][component], inverse_response[axis], 0.0f);
        }
        vector_row(words, 2 + component, Vector3{column[0], column[1], column[2]}, carry[component]);
    }
    // Row 5 is the newly cleared accumulated impulse, including its carry.
    const float separation = rs::mul_add(prepared.separation_projection[0], inverse_response[0], 0.0f);
    const float restitution = rs::mul_add(prepared.restitution_projection[0], inverse_response[0], 0.0f);
    std::array<float, 3> predicted{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        predicted[axis] = rs::mul_add(prepared.predicted_separation_projection[axis], inverse_response[axis], 0.0f);
    }
    float normal_adjustment = restitution >= 0.0f ? 0.0f : separation + restitution;
    if (separation >= 0.0f) {
        normal_adjustment = restitution;
    }
    if (0.0f >= predicted[0]) {
        normal_adjustment = 0.0f;
    }
    float_row(words, 6,
              {predicted[0] - normal_adjustment, predicted[1] - 0.0f, predicted[2] - 0.0f, separation});
    const std::array<std::uint32_t, 3> identifiers{prepared.body_ids[0], prepared.body_ids[1], prepared.contact_tag};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        vector_row(words, 7 + axis * 3, prepared.axes[axis], identifiers[axis]);
        auto a = prepared.angular_response_a[axis];
        auto b = prepared.angular_response_b[axis];
        if (axis == 0) {
            a[3] = prepared.inverse_mass[0];
            b[3] = prepared.inverse_mass[1];
        }
        float_row(words, 8 + axis * 3, a);
        float_row(words, 9 + axis * 3, b);
    }
    return words;
}

}  // namespace s3::physics::solver::contact_build::publication
