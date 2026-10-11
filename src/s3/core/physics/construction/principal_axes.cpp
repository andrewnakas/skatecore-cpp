// Port of skate-core/src/physics/construction/principal_axes.rs.
#include "s3/core/physics/construction/principal_axes.hpp"

#include <cstddef>
#include <utility>

#include "s3/core/physics/mass.hpp"

namespace s3::physics::mass::principal_axes {

namespace {

/// 82AE7AD8. The zero off-diagonal path is exactly the identity rotation.
std::pair<float, float> rotation(const Matrix3& matrix, std::size_t row, std::size_t column) {
    const float off_diagonal = matrix[row][column];
    if (off_diagonal == 0.0f) {
        return {0.0f, 1.0f};
    }
    const float difference = matrix[column][column] - matrix[row][row];
    const float theta = refined_reciprocal(off_diagonal) * (difference * 0.5f);
    const float length = rs::sqrt(rs::mul_add(theta, theta, 1.0f));
    const float tangent = theta > 0.0f ? 1.0f / (length + theta) : -1.0f / (length - theta);
    const float cosine = 1.0f / rs::sqrt(rs::mul_add(tangent, tangent, 1.0f));
    return {cosine * tangent, cosine};
}

/// 82AE7C30: independently rounded products for the subtraction and one FMA
/// for the addition. Combining the subtraction into an FMA changes the port.
void rotate_columns(Matrix3& matrix, std::size_t a, std::size_t b, float sine, float cosine) {
    for (auto& row : matrix) {
        const float left = row[a];
        const float right = row[b];
        row[a] = left * cosine - right * sine;
        row[b] = rs::mul_add(left, sine, right * cosine);
    }
}

/// 82AE7FD0 applies the matching rotation across the two matrix rows.
void rotate_rows(Matrix3& matrix, std::size_t a, std::size_t b, float sine, float cosine) {
    const auto left = matrix[a];
    const auto right = matrix[b];
    for (std::size_t column = 0; column < 3; ++column) {
        matrix[a][column] = left[column] * cosine - right[column] * sine;
        matrix[b][column] = rs::mul_add(left[column], sine, right[column] * cosine);
    }
}

}  // namespace

Matrix3 diagonalize(Matrix3& inertia) {
    Matrix3 axes{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    int remaining = 10;
    while (true) {
        // 82AE84A4..8680: squared lower-triangle residual, relative to the
        // smallest squared diagonal. Preserve comparison and FMA ordering.
        const std::array<float, 3> diagonal{inertia[0][0] * inertia[0][0],
                                            inertia[1][1] * inertia[1][1],
                                            inertia[2][2] * inertia[2][2]};
        float smallest;
        if (diagonal[1] > diagonal[0]) {
            smallest = diagonal[2] > diagonal[0] ? diagonal[0] : diagonal[2];
        } else if (diagonal[2] > diagonal[1]) {
            smallest = diagonal[1];
        } else {
            smallest = diagonal[2];
        }
        const float residual =
            rs::mul_add(inertia[2][1], inertia[2][1],
                        rs::mul_add(inertia[1][0], inertia[1][0], inertia[2][0] * inertia[2][0]));
        if (!(residual > smallest * 1.0e-24f) || remaining == 0) {
            break;
        }
        for (std::size_t row = 1; row < 3; ++row) {
            for (std::size_t column = 0; column < row; ++column) {
                const auto [sine, cosine] = rotation(inertia, row, column);
                rotate_columns(axes, row, column, sine, cosine);
                rotate_columns(inertia, row, column, sine, cosine);
                rotate_rows(inertia, row, column, sine, cosine);
            }
        }
        remaining -= 1;
    }
    return axes;
}

}  // namespace s3::physics::mass::principal_axes
