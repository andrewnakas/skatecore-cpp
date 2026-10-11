// Port of skate-core/src/physics/construction/mass_moments.rs (module physics::mass::mass_moments).
// Volume moments accumulated by TU3 82AE6CB8/82AE6D88/82AE6F88 and reduced
// to a local principal frame by 82AE7508. This is the aggregate mass producer;
// geometry construction and requested-mass scaling remain separate stages.
#pragma once
#include <array>

#include "s3/core/math.hpp"
#include "s3/core/physics/construction/primitive_mass.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::mass::mass_moments {

using math::Basis3;
using math::Vector3;
using primitive_mass_mod::PrimitiveMass;
using rigid_body::RetailLocalMassFrame;

struct AggregateMassProperties {
    float volume = 0.0f;
    RetailLocalMassFrame local_mass_frame;
    Vector3 moments_per_unit_mass;
};
S3_DERIVE_EQ(AggregateMassProperties, volume, local_mass_frame, moments_per_unit_mass)

/// Integrals of [x,y,z,1] outer products, in column order. The last column
/// holds first moments and volume. Keeping all lanes preserves native sums.
struct MassMoments {
    std::array<std::array<float, 4>, 4> columns{};

    static const MassMoments ZERO;

    /// 82AE6CB8 converts primitive principal inertia into volume moments.
    static MassMoments from_primitive(PrimitiveMass primitive);

    /// 82AE6D88: H * moments * transpose(H). The source explicitly sets the
    /// basis fourth lanes to zero and the translation fourth lane to one.
    void transform(Basis3 basis, Vector3 translation);

    /// 82AE6F88 adds successful children in volume order, without weighting or
    /// normalizing them individually. Collision-enable flags are not read here.
    void add(MassMoments child);

    /// 82AE7508 mutates the aggregate into its center-of-mass frame before
    /// diagonalizing its inertia. Preserve this mutation and native axis order.
    AggregateMassProperties principal_properties();
};
inline const MassMoments MassMoments::ZERO = MassMoments{};
S3_DERIVE_EQ(MassMoments, columns)

}  // namespace s3::physics::mass::mass_moments
