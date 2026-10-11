// Port of skate-core/src/physics/solver/joint_data.rs.
#include "s3/core/physics/solver/joint_data.hpp"

#include "s3/core/rs.hpp"

namespace s3::physics::solver::joint::data {

namespace {
Vector3 point_correction(const ReactionState& reaction, Vector3 arm) {
    const float x = reaction.angular[0];
    const float y = reaction.angular[1];
    const float z = reaction.angular[2];
    return Vector3{
        rs::mul_add(-z, arm.y, rs::mul_add(y, arm.z, reaction.linear[0])),
        rs::mul_add(-x, arm.z, rs::mul_add(z, arm.x, reaction.linear[1])),
        rs::mul_add(-y, arm.x, rs::mul_add(x, arm.y, reaction.linear[2])),
    };
}

Vector3 torque_at_offset(Vector3 arm, Vector3 impulse, Vector3 angular_impulse) {
    return Vector3{
        rs::mul_add(-arm.z, impulse.y, rs::mul_add(arm.y, impulse.z, angular_impulse.x)),
        rs::mul_add(-arm.x, impulse.z, rs::mul_add(arm.z, impulse.x, angular_impulse.y)),
        rs::mul_add(-arm.y, impulse.x, rs::mul_add(arm.x, impulse.y, angular_impulse.z)),
    };
}
}  // namespace

Column read_column(Slice<std::uint32_t> words, std::size_t byte_offset) {
    Column out{};
    for (std::size_t component = 0; component < 4; ++component) {
        out[component] = rs::from_bits(words[byte_offset / 4 + component]);
    }
    return out;
}

void write_column(Slice<std::uint32_t> words, std::size_t byte_offset, Column values) {
    S3_ASSERT(byte_offset / 4 + 4 <= words.size());
    for (std::size_t i = 0; i < 4; ++i) words[byte_offset / 4 + i] = rs::to_bits(values[i]);
}

ReactionState ReactionState::decode(Slice<std::uint32_t> words) {
    S3_ASSERT(words.size() == 16);
    return ReactionState{read_column(words, 0), read_column(words, 32)};
}

ConstraintGeometry ConstraintGeometry::decode(Slice<std::uint32_t> words) {
    S3_ASSERT(words.size() == 96);
    ConstraintGeometry g;
    g.arm_a = xyz(read_column(words, 0));
    g.arm_b = xyz(read_column(words, 16));
    for (std::size_t axis = 0; axis < 3; ++axis) {
        g.linear_projection[axis] = read_column(words, 64 + axis * 32);
        g.angular_projection[axis] = read_column(words, 80 + axis * 32);
        g.linear_axes[axis] = read_column(words, 192 + axis * 16);
        g.angular_axes[axis] = read_column(words, 240 + axis * 16);
        g.inverse_inertia_a[axis] = read_column(words, 288 + axis * 16);
        g.inverse_inertia_b[axis] = read_column(words, 336 + axis * 16);
    }
    return g;
}

std::pair<Vector3, Vector3> ConstraintGeometry::relative_corrections(const ReactionState& a,
                                                                     const ReactionState& b) const {
    const Vector3 point_a = point_correction(a, arm_a);
    const Vector3 point_b = point_correction(b, arm_b);
    return {
        Vector3{point_b.x - point_a.x, point_b.y - point_a.y, point_b.z - point_a.z},
        Vector3{b.angular[0] - a.angular[0], b.angular[1] - a.angular[1], b.angular[2] - a.angular[2]},
    };
}

void ConstraintGeometry::apply_impulse_changes(ReactionState& a, ReactionState& b, Vector3 linear_change,
                                               Vector3 angular_change) const {
    const Column linear_impulse = project_correction(linear_axes, linear_change, Column{});
    const Vector3 angular_impulse = xyz(project_correction(angular_axes, angular_change, Column{}));
    const Vector3 torque_a = torque_at_offset(arm_a, xyz(linear_impulse), angular_impulse);
    const Vector3 torque_b = torque_at_offset(arm_b, xyz(linear_impulse), angular_impulse);
    const float inverse_mass_a = inverse_inertia_a[0][3];
    const float inverse_mass_b = inverse_inertia_b[0][3];
    // Native publication order: linear A, linear B, angular A, angular B.
    for (std::size_t component = 0; component < 4; ++component) {
        const float impulse = linear_impulse[component];
        a.linear[component] = rs::mul_add(impulse, inverse_mass_a, a.linear[component]);
        b.linear[component] = rs::mul_add(-impulse, inverse_mass_b, b.linear[component]);
    }
    a.angular = project_correction(inverse_inertia_a, torque_a, a.angular);
    for (std::size_t component = 0; component < 4; ++component) {
        const float after_x = rs::mul_add(-inverse_inertia_b[0][component], torque_b.x, b.angular[component]);
        const float after_y = rs::mul_add(-inverse_inertia_b[1][component], torque_b.y, after_x);
        b.angular[component] = rs::mul_add(-inverse_inertia_b[2][component], torque_b.z, after_y);
    }
}

void publish_reactions(const ReactionState& a, const ReactionState& b, Slice<std::uint32_t> words_a,
                       Slice<std::uint32_t> words_b) {
    write_column(words_a, 0, a.linear);
    write_column(words_b, 0, b.linear);
    write_column(words_a, 32, a.angular);
    write_column(words_b, 32, b.angular);
}

Column project_correction(const std::array<Column, 3>& columns, Vector3 correction, Column initial) {
    Column out{};
    for (std::size_t component = 0; component < 4; ++component) {
        const float after_x = rs::mul_add(columns[0][component], correction.x, initial[component]);
        const float after_y = rs::mul_add(columns[1][component], correction.y, after_x);
        out[component] = rs::mul_add(columns[2][component], correction.z, after_y);
    }
    return out;
}

}  // namespace s3::physics::solver::joint::data
