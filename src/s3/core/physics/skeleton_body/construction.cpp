// Port of skate-core/src/physics/skeleton_body/construction.rs.
// SkeletonBody::Init82BE4900, head82BE5428 and extra capsules82BE5180.
#include <vector>

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_body::construction {

using mass::aggregate_mass_properties;
using mass::MassMoments;
using mass::PartMassInput;
using mass::primitive_mass;
using mass::primitive_mass_properties;
using mass::RETAIL_UNBOUNDED_VELOCITY;
using rigid_body::RetailLocalMassFrame;
using skeleton_animation_record::SkeletonAnimationMasses;

namespace {

rs::Result<RetailBodyMassProperties, const char*> mass_properties(MassShape shape,
                                                                  std::optional<HatGeometry> hat,
                                                                  float mass) {
    if (hat) {
        // Init82BE5428 uses cylinder first, then the head sphere. Preserve
        // accumulation order and the later identity mass-frame override.
        const auto cylinder = primitive_mass(MassShape::Cylinder{hat->radius, hat->half_length, 0.0f});
        if (!cylinder) return rs::make_err("Invalid skeleton hat primitive");
        MassMoments moments = MassMoments::from_primitive(*cylinder);
        moments.transform(hat->basis, hat->translation);
        const auto head = primitive_mass(shape);
        if (!head) return rs::make_err("Invalid head primitive");
        moments.add(MassMoments::from_primitive(*head));
        RetailBodyMassProperties properties =
            aggregate_mass_properties(moments, mass, RETAIL_UNBOUNDED_VELOCITY, 0.0f);
        properties.local_mass_frame = RetailLocalMassFrame::IDENTITY;
        return properties;
    }
    const auto properties =
        primitive_mass_properties(PartMassInput{shape, mass}, RETAIL_UNBOUNDED_VELOCITY, 0.0f);
    if (!properties) return rs::make_err("Invalid skeleton primitive");
    return *properties;
}

/// DebugMultiplyInertiaTensor82E0B070 / SetSphericalInertiaTensor82E0B110.
void adjust_inertia(RetailBodyMassProperties& properties, std::uint32_t kind, float factor) {
    auto& d = properties.dynamics;
    if (kind == 1) {
        float reciprocal = native_arithmetic::reciprocal_estimate(factor);
        for (int i = 0; i < 2; ++i) {
            reciprocal = rs::mul_add(reciprocal, rs::mul_add(-reciprocal, factor, 1.0f), reciprocal);
        }
        d.inverse_tensor = Vector3{d.inverse_tensor.x * reciprocal, d.inverse_tensor.y * reciprocal,
                                   d.inverse_tensor.z * reciprocal};
        const float xy = d.inverse_tensor.x < d.inverse_tensor.y ? d.inverse_tensor.x : d.inverse_tensor.y;
        const float xyz = xy < d.inverse_tensor.z ? xy : d.inverse_tensor.z;
        d.spherical = 1.0f / xyz;
    } else if (kind == 2) {
        const float reciprocal = 1.0f / (factor * 3.0f);
        const float value = ((d.inverse_tensor.x + d.inverse_tensor.y) + d.inverse_tensor.z) * reciprocal;
        d.inverse_tensor = Vector3{value, value, value};
        d.spherical = 1.0f / value;
    }
}

}  // namespace

HatGeometry HatGeometry::from_offsets(float radius, float thickness, Vector3 angles, Vector3 translation) {
    const auto [sx, cx] = trigonometry::sin_cos(angles.x);
    const auto [sy, cy] = trigonometry::sin_cos(angles.y);
    const auto [sz, cz] = trigonometry::sin_cos(angles.z);
    const float cxsz = cx * sz;
    const float sxsz = sx * sz;
    const float sxcz = sx * cz;
    const float cxcz = cx * cz;
    HatGeometry out;
    out.radius = radius;
    out.half_length = thickness * 0.5f;
    out.translation = translation;
    out.basis = Basis3{{{
        {cy * cz, cy * sz, -sy},
        {sy * sxcz - cxsz, rs::mul_add(sy, sxsz, cxcz), cy * sx},
        {rs::mul_add(sy, cxcz, sxsz), sy * cxsz - sxcz, cy * cx},
    }}};
    return out;
}

rs::Result<SkeletonBodyDefinition, const char*> SkeletonBodyDefinition::new_(
    std::array<Vector3, ANIMATION_PART_COUNT> sizes, std::array<BoneSettings, ANIMATION_PART_COUNT> bones,
    SkeletonBodySettings settings, std::optional<HatGeometry> hat) {
    std::vector<SkeletonPart> parts;
    parts.reserve(PART_COUNT);
    for (std::size_t i = 0; i < ANIMATION_PART_COUNT; ++i) {
        const BoneSettings b = bones[i];
        const Vector3 size = sizes[i];
        MassShape shape;
        if (i == 0) {
            shape = MassShape::Capsule{settings.root_radius, settings.root_half_length};
        } else if ((i == 1 && hat.has_value()) || b.volume_type == 1) {
            const float x = size.x * 0.5f;
            const float y = size.y * 0.5f;
            shape = MassShape::Sphere{(x - y >= 0.0f ? x : y) * b.volume_scalar};
        } else {
            switch (b.volume_type) {
                case 0: {
                    const float scalar = b.volume_scalar * 0.5f;
                    shape = MassShape::RoundedBox{Vector3{size.x * scalar, size.y * scalar, size.z * scalar},
                                                  0.0f};
                    break;
                }
                case 2: {
                    const float radius = (size.x * settings.capsule_radius_scalar) * 0.5f;
                    const float length = size.z * 0.5f - radius;
                    shape = MassShape::Capsule{
                        b.volume_scalar * radius,
                        ((-length >= 0.0f ? 0.0f : length) * settings.capsule_length_scalar) * b.volume_scalar,
                    };
                    break;
                }
                default:
                    return rs::make_err("Unresolved non-root skeleton volume type");
            }
        }
        const float base_mass =
            i == 0 ? rs::from_bits(0x3A83126Fu) : settings.density * ((size.x * size.y) * size.z);
        const std::optional<HatGeometry> head_hat = i == 1 ? hat : std::nullopt;
        const float animated_mass = b.mass_factor * base_mass;
        const float ragdoll_mass = b.ragdoll_mass_factor * base_mass;
        auto animated_result = mass_properties(shape, head_hat, animated_mass);
        if (animated_result.is_err()) return rs::make_err(animated_result.unwrap_err());
        RetailBodyMassProperties animated = animated_result.unwrap();
        auto ragdoll_result = mass_properties(shape, head_hat, ragdoll_mass);
        if (ragdoll_result.is_err()) return rs::make_err(ragdoll_result.unwrap_err());
        const RetailBodyMassProperties ragdoll = ragdoll_result.unwrap();
        adjust_inertia(animated, settings.inertia_multiply_type, settings.inertia_factor);
        parts.push_back(SkeletonPart{
            shape,
            head_hat,
            animated,
            ragdoll,
            1.0f / animated_mass,
            settings.ragdoll_inverse_mass_factor / ragdoll_mass,
        });
    }
    //82BE4F80..4FC4 passes these independent radius/half-length/mass words.
    const std::array<std::array<std::uint32_t, 3>, 2> extras{{
        {0x3E6B851Fu, 0x3DCCCCCDu, 0x3DCCCCCDu},
        {0x3EB851ECu, 0x3E851EB8u, 0x3C23D70Au},
    }};
    for (const auto& [radius, half_length, mass_bits] : extras) {
        const float mass = rs::from_bits(mass_bits);
        const MassShape shape = MassShape::Capsule{rs::from_bits(radius), rs::from_bits(half_length)};
        auto result = mass_properties(shape, std::nullopt, mass);
        if (result.is_err()) return rs::make_err(result.unwrap_err());
        RetailBodyMassProperties properties = result.unwrap();
        properties.dynamics.maximum_linear_velocity = rs::from_bits(0x41EFFFFFu);
        properties.dynamics.maximum_angular_velocity = rs::from_bits(0x41EFFFFFu);
        adjust_inertia(properties, 1, 5.0f);
        parts.push_back(SkeletonPart{shape, std::nullopt, properties, properties, 1.0f / mass, 1.0f / mass});
    }
    if (parts.size() != PART_COUNT) return rs::make_err("Skeleton part count");
    SkeletonBodyDefinition out;
    for (std::size_t i = 0; i < PART_COUNT; ++i) out.parts[i] = parts[i];
    std::array<std::uint32_t, ANIMATION_PART_COUNT> volume_types{};
    for (std::size_t i = 0; i < ANIMATION_PART_COUNT; ++i) volume_types[i] = bones[i].volume_type;
    out.animation_masses = SkeletonAnimationMasses::from_bone_data(sizes, volume_types, hat.has_value());
    out.bones = bones;
    return out;
}

}  // namespace s3::physics::skeleton_body::construction
