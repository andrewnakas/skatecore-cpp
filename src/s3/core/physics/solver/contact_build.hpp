// Port of skate-core/src/physics/solver/contact_build.rs.
// TU3 ContactBatchBuild `82AE10C8`: contact geometry, mass response and targets.
//
// Arithmetic is reconstructed for finite inputs and finite intermediates. The
// reciprocal response uses the isolated accepted Xenon `vrefp` approximation;
// callers can still inject captured hardware results through
// [`ContactMassResponse`] without changing the recovered builder order.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/rs.hpp"

namespace s3::physics::solver::contact_build {

using math::Vector3;

/// Complete preparation before the native reciprocal-estimate operation.
struct ContactPreparation {
    std::array<Vector3, 2> arms{};
    std::array<Vector3, 3> axes{};
    std::array<bool, 2> active{};
    std::array<float, 2> inverse_mass{};
    std::array<Vector3, 2> point_acceleration{};
    /// XYZ inverse-inertia response; W is the native packed carry value.
    std::array<std::array<float, 4>, 3> angular_response_a{};
    std::array<std::array<float, 4>, 3> angular_response_b{};
    /// Angular response plus both inverse masses, in normal/tangent order.
    std::array<float, 3> effective_mass{};
    std::array<float, 3> separation_projection{};
    std::array<float, 3> restitution_projection{};
    std::array<float, 3> predicted_separation_projection{};
    // private in Rust
    std::array<std::uint32_t, 2> reaction_ids{};
    std::array<std::uint32_t, 2> body_ids{};
    std::uint32_t static_friction_bits = 0;
    std::uint32_t dynamic_friction_bits = 0;
    std::uint32_t contact_tag = 0;
    std::uint32_t combined_state_bit_8 = 0;
};
S3_DERIVE_EQ(ContactPreparation, arms, axes, active, inverse_mass, point_acceleration,
             angular_response_a, angular_response_b, effective_mass, separation_projection,
             restitution_projection, predicted_separation_projection, reaction_ids, body_ids,
             static_friction_bits, dynamic_friction_bits, contact_tag, combined_state_bit_8)

/// `enum ContactBuildError { ReciprocalEstimateUnavailable { effective_mass } }`.
struct ContactBuildError {
    enum class Kind { ReciprocalEstimateUnavailable };
    Kind kind = Kind::ReciprocalEstimateUnavailable;
    std::array<float, 3> effective_mass{};
};
S3_DERIVE_EQ(ContactBuildError, kind, effective_mass)

/// Supplies the three native `vrefp` results without changing their arguments.
class ContactMassResponse {
public:
    virtual ~ContactMassResponse() = default;
    virtual rs::Result<std::array<float, 3>, ContactBuildError> inverse_effective_mass(
        std::array<float, 3> effective_mass) = 0;
};

class UnavailableContactMassResponse : public ContactMassResponse {
public:
    rs::Result<std::array<float, 3>, ContactBuildError> inverse_effective_mass(
        std::array<float, 3> effective_mass) override {
        return rs::make_err(
            ContactBuildError{ContactBuildError::Kind::ReciprocalEstimateUnavailable, effective_mass});
    }
};

/// Default host implementation of the three `vrefp` lanes at `0x82AE158C`.
/// The approximation is isolated in `native_arithmetic` so hardware traces can
/// replace it without changing contact geometry or publication.
class NativeContactMassResponse : public ContactMassResponse {
public:
    rs::Result<std::array<float, 3>, ContactBuildError> inverse_effective_mass(
        std::array<float, 3> effective_mass) override;
};

ContactPreparation prepare_contact(const std::array<std::uint32_t, 64>& record, float time_step);

/// Builds all rows privately and publishes only after mass response succeeds.
rs::Result<rs::Unit, ContactBuildError> build_with_response(std::array<std::uint32_t, 64>& record,
                                                            float time_step,
                                                            ContactMassResponse& response);

/// pub(super)
void build(std::array<std::uint32_t, 64>& record, float time_step);

}  // namespace s3::physics::solver::contact_build

#include "s3/core/physics/solver/contact_build/geometry.hpp"
#include "s3/core/physics/solver/contact_build/input.hpp"
#include "s3/core/physics/solver/contact_build/publication.hpp"
