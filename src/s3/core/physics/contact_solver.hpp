// Port of skate-core/src/physics/contact_solver.rs.
// TU3 ContactBatchBuild and the shared Horus contact solver.
// Typed contacts are encoded only at the compiled-row boundary. Contact
// iteration has a readable direct-source implementation; the native builder's
// reciprocal-estimate dependency remains unavailable.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/math.hpp"
#include "s3/core/physics/contact.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/slice.hpp"

namespace s3::physics::contact_solver {

using math::Vector3;

namespace tu3 {
inline constexpr std::uint32_t CONTACT_BATCH_BUILD = 0x82AE10C8u;
inline constexpr std::uint32_t ITERATIVE_CONSTRAINT_SOLVER = 0x82AE27D0u;
}  // namespace tu3
inline constexpr std::uint32_t ACTIVE_BODY = 4;

struct RetailContactJacobian {
    /// pub(crate) in Rust.
    std::array<std::uint32_t, 64> words{};
    std::size_t reaction_index_a = 0;
    std::size_t reaction_index_b = 0;

    /// Native compiled layout, for inspection and independent comparisons.
    const std::array<std::uint32_t, 64>& words_() const { return words; }
    std::array<float, 4> accumulated_impulse() const { return lanes(5); }
    /// Weighted velocity targets in xyz and position-only target in w.
    std::array<float, 4> target_impulse() const { return lanes(6); }
    float static_friction() const { return rs::from_bits(words[15]); }
    float dynamic_friction() const { return rs::from_bits(words[19]); }
    std::array<Vector3, 3> angular_response_a() const {
        return {xyz(8), xyz(11), xyz(14)};
    }
    std::array<Vector3, 3> angular_response_b() const {
        return {xyz(9), xyz(12), xyz(15)};
    }

private:
    std::array<float, 4> lanes(std::size_t vector) const {
        return {rs::from_bits(words[vector * 4]), rs::from_bits(words[vector * 4 + 1]),
                rs::from_bits(words[vector * 4 + 2]), rs::from_bits(words[vector * 4 + 3])};
    }
    Vector3 xyz(std::size_t vector) const {
        const auto v = lanes(vector);
        return Vector3{v[0], v[1], v[2]};
    }
};
S3_DERIVE_EQ(RetailContactJacobian, words, reaction_index_a, reaction_index_b)

/// Requests native contact construction. The unresolved reciprocal estimate
/// currently stops this call explicitly; callers must not treat compiled-row
/// solver tests as evidence that this producer is ready for gameplay.
/// Each input body workspace must include forces accumulated for this tick.
RetailContactJacobian build_contact_jacobian(contact::RetailContact contact, float time_step);

/// Contact-only entry point uses the same kernel and reaction layout as the
/// complete contact -> joint -> drive iteration. Friction reads the preceding
/// normal impulse, before this pass publishes its new normal impulse.
void solve_contact_jacobians(Slice<RetailContactJacobian> contacts,
                             Slice<rigid_body::RetailReactionCorrections> reactions,
                             std::uint32_t maximum_iterations);
void solve_contact_jacobian_iteration(Slice<RetailContactJacobian> contacts,
                                      Slice<rigid_body::RetailReactionCorrections> reactions);

}  // namespace s3::physics::contact_solver
