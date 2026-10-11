// Port of skate-core/src/physics/skeleton_body/construction.rs (included via skeleton_body.hpp).
// SkeletonBody::Init82BE4900, head82BE5428 and extra capsules82BE5180.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/physics/mass.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"

namespace s3::physics::skeleton_body::construction {

using math::Basis3;
using math::Vector3;
using mass::MassShape;
using rigid_body::RetailBodyMassProperties;

struct BoneSettings {
    float mass_factor = 0.0f;
    float ragdoll_mass_factor = 0.0f;
    bool has_collision = false;
    bool use_root_drive = false;
    std::uint32_t volume_type = 0;
    float volume_scalar = 0.0f;
    std::uint32_t num_parents = 0;
};

struct SkeletonBodySettings {
    /// Native MassOfSkeleton multiplies each original bone-box volume. It is
    /// not a requested total body mass and must not normalize the part masses.
    float density = 0.0f;
    float root_radius = 0.0f;
    float root_half_length = 0.0f;
    float capsule_radius_scalar = 0.0f;
    float capsule_length_scalar = 0.0f;
    float ragdoll_inverse_mass_factor = 0.0f;
    std::uint32_t inertia_multiply_type = 0;
    float inertia_factor = 0.0f;
};

/// Authored cylinder and transform from physics_hat, before head aggregation.
struct HatGeometry {
    float radius = 0.0f;
    float half_length = 0.0f;
    Basis3 basis;
    Vector3 translation;

    ///82BE55A0..569C: authored Euler angles feed XMVectorSinCos directly.
    static HatGeometry from_offsets(float radius, float thickness, Vector3 angles, Vector3 translation);
};

struct SkeletonPart {
    /// Part1 can additionally contain the separately described hat cylinder.
    MassShape shape;
    std::optional<HatGeometry> hat;
    RetailBodyMassProperties animated;
    RetailBodyMassProperties ragdoll;
    float inverse_mass_animated = 0.0f;
    float inverse_mass_ragdoll = 0.0f;
};

struct SkeletonBodyDefinition {
    std::array<SkeletonPart, PART_COUNT> parts{};
    std::array<BoneSettings, ANIMATION_PART_COUNT> bones{};
    skeleton_animation_record::SkeletonAnimationMasses animation_masses;

    static rs::Result<SkeletonBodyDefinition, const char*> new_(
        std::array<Vector3, ANIMATION_PART_COUNT> sizes,
        std::array<BoneSettings, ANIMATION_PART_COUNT> bones, SkeletonBodySettings settings,
        std::optional<HatGeometry> hat);
};

}  // namespace s3::physics::skeleton_body::construction

namespace s3::physics::skeleton_body {
using construction::BoneSettings;
using construction::HatGeometry;
using construction::SkeletonBodyDefinition;
using construction::SkeletonBodySettings;
using construction::SkeletonPart;
}  // namespace s3::physics::skeleton_body
