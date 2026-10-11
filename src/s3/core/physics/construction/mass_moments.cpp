// Port of skate-core/src/physics/construction/mass_moments.rs.
#include "s3/core/physics/construction/mass_moments.hpp"

#include "s3/core/physics/construction/principal_axes.hpp"
#include "s3/core/physics/mass.hpp"

namespace s3::physics::mass::mass_moments {

namespace {
using V4 = std::array<float, 4>;
using M4 = std::array<V4, 4>;

V4 product(const M4& matrix, const V4& vector) {
    return rs::from_fn<4>([&](std::size_t i) {
        return rs::mul_add(
            matrix[3][i], vector[3],
            rs::mul_add(matrix[2][i], vector[2],
                        rs::mul_add(matrix[1][i], vector[1], matrix[0][i] * vector[0])));
    });
}

float select_nonnegative(float selector, float positive, float negative) {
    return selector >= 0.0f ? positive : negative;
}
}  // namespace

MassMoments MassMoments::from_primitive(PrimitiveMass primitive) {
    const Vector3 inertia = primitive.moments_per_unit_mass;
    const float volume = primitive.volume;
    const float x = (((inertia.z + inertia.y) - inertia.x) * volume) * 0.5f;
    const float y = (((inertia.z + inertia.x) - inertia.y) * volume) * 0.5f;
    const float z = (((inertia.y + inertia.x) - inertia.z) * volume) * 0.5f;
    return MassMoments{{{
        {x, 0.0f, 0.0f, 0.0f},
        {0.0f, y, 0.0f, 0.0f},
        {0.0f, 0.0f, z, 0.0f},
        {0.0f, 0.0f, 0.0f, volume},
    }}};
}

void MassMoments::transform(Basis3 basis, Vector3 translation) {
    M4 affine{};
    for (std::size_t c = 0; c < 3; ++c) {
        for (std::size_t r = 0; r < 3; ++r) affine[c][r] = basis.columns[c][r];
    }
    affine[3] = {translation.x, translation.y, translation.z, 1.0f};
    const M4 left = rs::map(columns, [&](const V4& column) { return product(affine, column); });
    columns = rs::from_fn<4>([&](std::size_t column) {
        return product(left, rs::from_fn<4>([&](std::size_t row) { return affine[row][column]; }));
    });
}

void MassMoments::add(MassMoments child) {
    for (std::size_t c = 0; c < 4; ++c) {
        for (std::size_t r = 0; r < 4; ++r) columns[c][r] += child.columns[c][r];
    }
}

AggregateMassProperties MassMoments::principal_properties() {
    const float volume = columns[3][3];
    const float inverse_volume = refined_reciprocal(volume);
    const Vector3 center{
        columns[3][0] * inverse_volume,
        columns[3][1] * inverse_volume,
        columns[3][2] * inverse_volume,
    };
    transform(RetailLocalMassFrame::IDENTITY.basis, Vector3{-center.x, -center.y, -center.z});
    const auto m = columns;
    const float xy = (m[1][0] + m[0][1]) * -0.5f;
    const float xz = (m[2][0] + m[0][2]) * -0.5f;
    const float yz = (m[2][1] + m[1][2]) * -0.5f;
    principal_axes::Matrix3 inertia{{
        {m[1][1] + m[2][2], xy, xz},
        {xy, m[0][0] + m[2][2], yz},
        {xz, yz, m[0][0] + m[1][1]},
    }};
    const auto axes = principal_axes::diagonalize(inertia);
    const float maximum_yz =
        select_nonnegative(inertia[1][1] - inertia[2][2], inertia[1][1], inertia[2][2]);
    const float maximum = select_nonnegative(inertia[0][0] - maximum_yz, inertia[0][0], maximum_yz);
    const float floor = maximum * 0.0050000004f;
    const auto moments = rs::from_fn<3>([&](std::size_t i) {
        return select_nonnegative(inertia[i][i] - floor, inertia[i][i], floor) * inverse_volume;
    });
    Basis3 basis;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) basis.columns[i][j] = axes[j][i];
    }
    return AggregateMassProperties{
        volume,
        RetailLocalMassFrame{basis, center},
        Vector3{moments[0], moments[1], moments[2]},
    };
}

}  // namespace s3::physics::mass::mass_moments
