// Port of skate-core/src/physics/joint_builder/arithmetic.rs.
#include "s3/core/physics/joint_builder.hpp"

#include "s3/core/physics/constraint_frames.hpp"
#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::joint_builder::arithmetic {

JointParameters decode_parameters(RetailJointParametersRaw raw) {
    return JointParameters{
        vector_from_words(raw.words, 0),
        vector_from_words(raw.words, 4),
        rs::from_bits(raw.words[8]),
        rs::from_bits(raw.words[9]),
        rs::from_bits(raw.words[12]),
        rs::from_bits(raw.words[13]),
        raw.words[14],
        raw.words[15],
    };
}

JointFrames decode_frames(RetailJointFramesRaw raw) {
    return JointFrames{
        quaternion_from_words(raw.words, 0),  vector_from_words(raw.words, 4),
        quaternion_from_words(raw.words, 8),  vector_from_words(raw.words, 12),
        quaternion_from_words(raw.words, 16),
    };
}

std::tuple<std::array<Vector3, 3>, std::array<float, 3>, std::array<float, 3>> angular_rows(
    JointParameters parameters, Basis3 basis_a, Basis3 basis_b,
    std::array<std::array<float, 3>, 3> relative, Rqd rqd) {
    const float finite_infinity = tu3::FINITE_INFINITY;
    // Native defaults are all three frame-B axes, including skipped limits.
    std::array<Vector3, 3> axes = basis_columns(basis_b);
    std::array<float, 3> low{-finite_infinity, -finite_infinity, -finite_infinity};
    std::array<float, 3> high{finite_infinity, finite_infinity, finite_infinity};

    switch (parameters.swing_mode) {
        case 0: {
            axes[1] = rqd.axes[1];
            axes[2] = rqd.axes[2];
            low[1] = 2.0f * rqd.relative.y;
            high[1] = low[1];
            low[2] = 2.0f * rqd.relative.z;
            high[2] = low[2];
            break;
        }
        case 1: {
            // 82AE4178..4188 skips the cone calculation when all splatted
            // cosine lanes compare >= the original 0.99999 guard.
            if (!(relative[0][0] >= tu3::NEAR_PARALLEL)) {
                const Vector3 first_a = basis_column(basis_a, 0);
                const Vector3 first_b = basis_column(basis_b, 0);
                const float perpendicular_z = relative[0][2];
                const float perpendicular_y = relative[0][1];
                const float squared =
                    rs::mul_add(perpendicular_y, perpendicular_y, perpendicular_z * perpendicular_z);
                float root = squared * constraint_frames::reciprocal_sqrt(squared);
                root = squared == 0.0f ? 0.0f : root;
                const float inverse_length = constraint_frames::reciprocal(root) * 1.0f;
                axes[1] = scale(cross(first_a, first_b), inverse_length);
                axes[2] = cross(axes[1], first_a);
                // The bound is written even while inside the cone, allowing
                // the predicted rate to reach the limit during this step.
                low[1] = (parameters.swing_threshold - relative[0][0]) * inverse_length;
            }
            break;
        }
        case 2:
        case 3: {
            axes[1] = basis_column(basis_b, 1);
            if (parameters.twist_mode == 0) {
                axes[2] = rqd.axes[2];
                low[2] = 2.0f * rqd.relative.z;
                high[2] = low[2];
            } else {
                axes[2] = cross(basis_column(basis_a, 0), axes[1]);
                low[2] = -relative[0][1];
                high[2] = low[2];
            }
            if (parameters.swing_mode == 2) {
                const float denominator = relative[0][2];
                // 82AE42F4..4304: equality skips the reciprocal and retains
                // both unbounded values. NaN is not equal to zero.
                if (denominator != 0.0f) {
                    const float correction = constraint_frames::reciprocal(denominator) *
                                             (parameters.swing_threshold - relative[0][0]);
                    if (denominator < 0.0f) {
                        high[1] = correction;
                    } else {
                        low[1] = correction;
                    }
                }
            }
            break;
        }
        default: {
            axes[1] = basis_column(basis_b, 1);
            axes[2] = basis_column(basis_b, 2);
            break;
        }
    }

    switch (parameters.twist_mode) {
        case 0: {
            axes[0] = rqd.axes[0];
            low[0] = 2.0f * rqd.relative.x;
            high[0] = low[0];
            break;
        }
        case 1: {
            axes[0] = basis_column(basis_a, 0);
            // Derived matrix-index form matching the emitted TU3 subtraction.
            const float denominator = relative[2][1] - relative[1][2];
            // 82AE4434..4444 also retains the unbounded interval at zero.
            if (denominator != 0.0f) {
                const float numerator = (1.0f + relative[0][0]) * parameters.twist_threshold -
                                        (relative[1][1] + relative[2][2]);
                const float correction = constraint_frames::reciprocal(denominator) * numerator;
                if (denominator < 0.0f) {
                    high[0] = correction;
                } else {
                    low[0] = correction;
                }
            }
            break;
        }
        default: {
            axes[0] = basis_column(basis_b, 0);
            break;
        }
    }

    return {axes, low, high};
}

Rqd create_rqd(RetailQuaternion world_a, RetailQuaternion world_b) {
    const auto rows = constraint_frames::quaternion_rows(world_a, world_b);
    return Rqd{rows.relative, rows.axes};
}

std::array<std::array<float, 3>, 3> relative_basis(Basis3 a, Basis3 b) {
    std::array<std::array<float, 3>, 3> out{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            out[row][column] = dot3(basis_column(a, row), basis_column(b, column));
        }
    }
    return out;
}

std::array<std::array<float, 3>, 3> scaled_projection_columns(std::array<Vector3, 3> axes,
                                                              std::array<float, 3> inverse_effective_mass) {
    std::array<std::array<float, 3>, 3> out{};
    for (std::size_t component = 0; component < 3; ++component) {
        for (std::size_t row = 0; row < 3; ++row) {
            out[component][row] = vector_to_array(axes[row])[component] * inverse_effective_mass[row];
        }
    }
    return out;
}

std::array<std::array<float, 4>, 3> packed_inertia_columns(RetailPackedWorldInverseInertia packed,
                                                           float inverse_mass) {
    return {{
        {packed.full.x, packed.full.y, packed.full.z, inverse_mass},
        {packed.full.y, packed.split.y, packed.split.z, inverse_mass},
        {packed.full.z, packed.split.z, packed.split.x, inverse_mass},
    }};
}

RetailPackedWorldInverseInertia zero_inertia() {
    return RetailPackedWorldInverseInertia{Vector3::ZERO, Vector3::ZERO};
}

Basis3 basis_from_quaternion_retail(RetailQuaternion q) { return constraint_frames::basis(q); }

RetailQuaternion quaternion_multiply(RetailQuaternion a, RetailQuaternion b) {
    return constraint_frames::compose(a, b);
}

Vector3 point_rate(Vector3 linear, Vector3 angular, Vector3 arm) {
    return constraint_frames::point_rate(linear, angular, arm);
}

std::array<float, 3> project(Vector3 vector, std::array<Vector3, 3> axes) {
    return rs::map(axes, [&](const Vector3& axis) { return dot3(vector, axis); });
}

Vector3 multiply_basis(Basis3 basis, Vector3 vector) {
    return constraint_frames::transform_direction(basis, vector);
}

std::array<Vector3, 3> basis_columns(Basis3 basis) {
    return {basis_column(basis, 0), basis_column(basis, 1), basis_column(basis, 2)};
}

Vector3 basis_column(Basis3 basis, std::size_t index) {
    return Vector3{basis.columns[index][0], basis.columns[index][1], basis.columns[index][2]};
}

std::array<float, 3> vector_to_array(Vector3 value) { return {value.x, value.y, value.z}; }

Vector3 cross(Vector3 left, Vector3 right) { return constraint_frames::cross(left, right); }

float dot3(Vector3 left, Vector3 right) {
    return native_arithmetic::dot3({left.x, left.y, left.z, 0.0f}, {right.x, right.y, right.z, 0.0f});
}

Vector3 add(Vector3 left, Vector3 right) { return Vector3{left.x + right.x, left.y + right.y, left.z + right.z}; }

Vector3 sub(Vector3 left, Vector3 right) { return Vector3{left.x - right.x, left.y - right.y, left.z - right.z}; }

Vector3 scale(Vector3 value, float scalar) { return Vector3{value.x * scalar, value.y * scalar, value.z * scalar}; }

float vmx_min(float left, float right) { return native_arithmetic::vector_min(left, right); }

float vmx_max(float left, float right) { return native_arithmetic::vector_max(left, right); }

}  // namespace s3::physics::joint_builder::arithmetic
