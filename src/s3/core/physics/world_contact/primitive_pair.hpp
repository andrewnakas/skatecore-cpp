// Port of skate-core/src/physics/world_contact/primitive_pair.rs.
//
// Simulation GP-pair contact query, original Skate 3 TU3 82AD43A8.
//
// 8277A508 calls this after its pair-culling bitmap and bounds checks, including
// the same-assembly branch at 8277B2C0. This is not PrimitivePairIntersect
// 82AD3CD8: that query has a squared-distance gate and different publication.
// Host geometry is already transformed into world space. Native GP dispatch,
// feature/prism order, normal refinement and radius application are retained.
#pragma once
#include <optional>

#include "s3/core/physics/world_contact/primitive_query.hpp"

namespace s3::physics::world_contact::primitive_pair {

using primitive_query::ContactPrimitive;
using primitive_query::PrimitiveContactManifold;

struct PrimitivePairSettings {
    /// Volume-record +112 for A/B in 8277A508, separate from shape fatness.
    float padding_a = 0.0f;
    float padding_b = 0.0f;
    /// Batch job +36, passed by 8277AB70; added after both volume paddings.
    float additional_padding = 0.0f;
    /// Native f2/f3 of 82AD43A8, used only for triangle feature correction.
    float edge_cos_bend_normal_threshold = 0.0f;
    float convexity_epsilon = 0.0f;

    /// Original skater/skater batch settings. SerializedAssembly::Create
    /// 82DC3AE4/82DC3B84 stores 82165A00 into every volume's padding (+112).
    /// Island::InitWorld 82765544 initializes additionalPadding to zero;
    /// 82DC1848 -> 82DC19E8 -> 8277B2A4 passes it to this query. The triangle
    /// constants are loaded by 8277ABEC..AC1C and forwarded at 8277A7DC..7EC.
    static PrimitivePairSettings skater_self_collision() {
        return PrimitivePairSettings{
            rs::from_bits(0x3D4CCCCDu), rs::from_bits(0x3D4CCCCDu), 0.0f,
            rs::from_bits(0x3F7FBE77u), rs::from_bits(0x3C23D70Au),
        };
    }
};

/// Contacts between two simulation volumes. The normal points from B toward A;
/// `points[i].a/b` belong to the corresponding input volume. Positive geometric
/// gaps inside the native padding are retained for the solver to classify.
std::optional<PrimitiveContactManifold> primitive_pair_contacts(const ContactPrimitive& primitive_a,
                                                                const ContactPrimitive& primitive_b,
                                                                PrimitivePairSettings settings);

}  // namespace s3::physics::world_contact::primitive_pair
