// Port of skate-core/src/physics/tests/mass_moments.rs.
#include "harness.hpp"
#include "s3/core/physics/mass.hpp"

namespace {
using namespace s3::physics::mass;

void close(float actual, float expected) {
    CHECK_MSG(s3::rs::abs(actual - expected) < 2.0e-5f, actual << " != " << expected);
}

S3_TEST(translated_primitive_recovers_center_without_parallel_axis_inertia) {
    MassMoments moments = MassMoments::from_primitive(PrimitiveMass{Vector3{2.0f, 3.0f, 4.0f}, 2.0f});
    const Basis3 identity{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    moments.transform(identity, Vector3{2.0f, -1.0f, 3.0f});
    const auto result = moments.principal_properties();
    CHECK_EQ(result.local_mass_frame.translation, (Vector3{2.0f, -1.0f, 3.0f}));
    close(result.moments_per_unit_mass.x, 2.0f);
    close(result.moments_per_unit_mass.y, 3.0f);
    close(result.moments_per_unit_mass.z, 4.0f);
}

S3_TEST(rotated_mass_reconstructs_independent_world_inertia) {
    const float sine = 0.6f;
    const float cosine = 0.8f;
    const Basis3 basis{{{{cosine, sine, 0.0f}, {-sine, cosine, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    MassMoments moments = MassMoments::from_primitive(PrimitiveMass{Vector3{2.0f, 3.0f, 4.0f}, 2.0f});
    moments.transform(basis, Vector3{0.0f, 0.0f, 0.0f});
    const auto result = moments.principal_properties();
    const auto columns = result.local_mass_frame.basis.columns;
    const std::array<float, 3> diagonal{result.moments_per_unit_mass.x, result.moments_per_unit_mass.y,
                                        result.moments_per_unit_mass.z};
    // R diag(2,3,4) R^T for the independently specified rotation.
    const float expected[3][3] = {{2.36f, -0.48f, 0.0f}, {-0.48f, 2.64f, 0.0f}, {0.0f, 0.0f, 4.0f}};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            float actual = -0.0f;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                actual += columns[axis][row] * diagonal[axis] * columns[axis][column];
            }
            close(actual, expected[row][column]);
        }
    }
}

S3_TEST(two_translated_equal_volumes_preserve_parallel_axis_contribution) {
    const PrimitiveMass primitive{Vector3{1.0f, 1.0f, 1.0f}, 3.0f};
    const Basis3 identity{{{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    MassMoments total = MassMoments::ZERO;
    for (float x : {-2.0f, 2.0f}) {
        MassMoments child = MassMoments::from_primitive(primitive);
        child.transform(identity, Vector3{x, 0.0f, 0.0f});
        total.add(child);
    }
    const auto result = total.principal_properties();
    CHECK_EQ(result.volume, 6.0f);
    close(result.moments_per_unit_mass.x, 1.0f);
    close(result.moments_per_unit_mass.y, 5.0f);
    close(result.moments_per_unit_mass.z, 5.0f);
}

S3_TEST(aggregate_finalizer_stores_inverse_frame_and_keeps_world_inertia) {
    const Basis3 rotation{{{{0.8f, 0.6f, 0.0f}, {-0.6f, 0.8f, 0.0f}, {0.0f, 0.0f, 1.0f}}}};
    const Vector3 center{2.0f, -1.0f, 3.0f};
    MassMoments moments = MassMoments::from_primitive(PrimitiveMass{Vector3{2.0f, 3.0f, 4.0f}, 2.0f});
    moments.transform(rotation, center);
    const auto result = aggregate_mass_properties(moments, 2.0f, 20.0f, 0.25f);
    const auto inverse = result.local_mass_frame;
    const std::array<float, 3> translation{inverse.translation.x, inverse.translation.y,
                                           inverse.translation.z};
    for (std::size_t row = 0; row < 3; ++row) {
        // Applying inverseBodyLTM to the authored COM must produce the origin.
        const float local_center = inverse.basis.columns[0][row] * center.x +
                                   inverse.basis.columns[1][row] * center.y +
                                   inverse.basis.columns[2][row] * center.z + translation[row];
        close(local_center, 0.0f);
    }
    const auto tensor = result.dynamics.inverse_tensor;
    const std::array<float, 3> principal{1.0f / tensor.x, 1.0f / tensor.y, 1.0f / tensor.z};
    // I_world = transpose(inverse_basis) * I_principal * inverse_basis.
    const float expected[3][3] = {{4.72f, -0.96f, 0.0f}, {-0.96f, 5.28f, 0.0f}, {0.0f, 0.0f, 8.0f}};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            float actual = -0.0f;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                actual += inverse.basis.columns[row][axis] * principal[axis] *
                          inverse.basis.columns[column][axis];
            }
            close(actual, expected[row][column]);
        }
    }
}

S3_TEST(finalizer_omits_source_negligible_frame_correction) {
    MassMoments moments = MassMoments::from_primitive(PrimitiveMass{Vector3{2.0f, 3.0f, 4.0f}, 2.0f});
    moments.transform(RetailLocalMassFrame::IDENTITY.basis, Vector3{0.0001f, 0.0f, 0.0f});
    const auto result = aggregate_mass_properties(moments, 2.0f, 20.0f, 0.0f);
    CHECK_EQ(result.local_mass_frame, RetailLocalMassFrame::IDENTITY);
}
}  // namespace
