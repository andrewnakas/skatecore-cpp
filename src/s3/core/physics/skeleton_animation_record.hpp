// Port of skate-core/src/physics/skeleton_animation_record.rs.
// Animation-record COM producer82BEBF88 and mass normalization82BEBAA8.
// Skeleton embeds this source state at6464, so com_to_deck_world4544 is the
// ProcessData82BD8918 input published from Skeleton11008 to Processed752.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::skeleton_animation_record {

using math::Vector3;

/// Native affine columns X,Y,Z,translation. Keep all four stored lanes;
/// UpdateAnimRecord copies24 complete transforms, not just the positions.
using AnimationPartTransform = std::array<std::array<float, 4>, 4>;
inline constexpr AnimationPartTransform IDENTITY = {{
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
}};

/// Forward PhysicsParamBoneData frame from SkeletonData::Init82BD6E24..6E98.
/// The same sqrt(2), fused negative product and lane permutations are used by
/// RigidBody::DynamicUpdate. Keep the otherwise unused fourth rotation lanes:
/// native permute masks822FB8A0/B0/C0 duplicate Z.z, Y.x and X.y respectively.
/// This is the forward frame, not the inverse stored by the later init slice.
AnimationPartTransform physics_bone_frame(std::array<float, 4> quaternion,
                                          std::array<float, 4> translation);

/// ProcessData82BD8BB4..8CAC: gather each named global animation bone, then
/// right-multiply its forward physics frame. Native maps names to hierarchy
/// indices in82BD6CB4..6CE4; the host loader provides those verified indices.
/// The result is the input to the following skeleton IK and adjustment stages,
/// which must run before UpdateAnimRecord for the final physical pose.
/// (`global_bones` is a Rust slice: pointer + length.)
rs::Result<std::array<AnimationPartTransform, 24>, const char*> map_animation_parts(
    const AnimationPartTransform* global_bones, std::size_t global_bones_len,
    const std::array<std::size_t, 24>& bone_indices,
    const std::array<AnimationPartTransform, 24>& physics_frames);
inline rs::Result<std::array<AnimationPartTransform, 24>, const char*> map_animation_parts(
    const std::vector<AnimationPartTransform>& global_bones,
    const std::array<std::size_t, 24>& bone_indices,
    const std::array<AnimationPartTransform, 24>& physics_frames) {
    return map_animation_parts(global_bones.data(), global_bones.size(), bone_indices,
                               physics_frames);
}

struct SkeletonAnimationMasses {
    /// Native misleadingly calls these part masses. They are bone-box volumes
    /// before density and animated/ragdoll physical mass scaling.
    std::array<float, 24> part_weights{};
    float total = 0.0f;
    std::array<float, 24> fractional{};

    /// Full weight assignment slice82BE4900/82BE5428. Board part0 is weight0;
    /// other parts use original PhysicsParamBoneData.size32, not collider size.
    /// Shapes0/1/2 all use the same box product. Native unrecognized shapes>=3
    /// retain the iteration's initialized0. A hat on head1 takes its own branch
    /// and still uses the original head box product, excluding the hat volume.
    static SkeletonAnimationMasses from_bone_data(std::array<Vector3, 24> sizes,
                                                  std::array<std::uint32_t, 24> collision_shapes,
                                                  bool head_has_hat);

    ///82BEBAA8: ordered24 scalar additions, one scalar single-precision divide,
    /// then24 multiplies. No clamping, epsilon, density or re-normalization.
    static SkeletonAnimationMasses normalize(std::array<float, 24> part_weights);
};

struct SkeletonAnimationRecord {
    // Default: Constructor82BEB8C8 initializes24 animation transforms via82BF30C0,
    // then all four vectors and resetScalar5224 to0.
    std::array<AnimationPartTransform, 24> pose = filled_identity();
    std::array<float, 4> centre_of_mass{};
    std::array<float, 4> centre_of_mass_delta{};
    std::array<float, 4> com_to_deck_world{};
    std::array<float, 4> com_to_deck_world_delta{};
    float reset_scalar = 0.0f;

    static SkeletonAnimationRecord default_() { return SkeletonAnimationRecord{}; }

    /// Animation fields of SkeletonState::Reset82BEBBB0. The function leaves
    /// animation transforms, fractional masses and resetScalar unchanged.
    void reset_history();

    void update(const std::array<AnimationPartTransform, 24>& pose,
                const AnimationPartTransform& animation_to_board,
                const SkeletonAnimationMasses& masses);

    Vector3 com_to_deck() const;

private:
    static std::array<AnimationPartTransform, 24> filled_identity() {
        std::array<AnimationPartTransform, 24> out{};
        out.fill(IDENTITY);
        return out;
    }
};

std::array<float, 4> transform_point(const AnimationPartTransform& transform,
                                     std::array<float, 4> point);

/// Native affine product used by ProcessData and board-offset application.
AnimationPartTransform compose_affine(const AnimationPartTransform& parent,
                                      const AnimationPartTransform& child);

}  // namespace s3::physics::skeleton_animation_record
