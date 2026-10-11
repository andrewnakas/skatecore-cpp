// Port of skate-core/src/animation/foot_ik/drive.rs.
// SkeletonIK::UpdateDriveFrames82BF0198 and its solve/mapping chain.
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include "s3/core/animation/foot_ik/math.hpp"
#include "s3/core/animation/foot_ik/status.hpp"
#include "s3/core/animation/foot_ik/transforms.hpp"
#include "s3/core/animation/foot_ik/two_bone.hpp"

namespace s3::animation::foot_ik::drive {

using Transform = physics::skeleton_animation_record::AnimationPartTransform;
using transforms::LimbBinding;
using transforms::LimbFrames;
using math::Vector;

struct Geometry {
    /// SkeletonData4704: nearest ancestor represented by a physical part.
    std::array<std::optional<std::size_t>, 24> parents{};
    /// SkeletonData1536: native cofactor inverse of the authored part frames.
    std::array<Transform, 24> inverse_part_frames{};

    /// SkeletonData Init82BD6C40 builds these inverses once from the real
    /// PhysicsParamBoneData frames. It does not assume an orthogonal basis.
    static rs::Result<Geometry, std::string> new_(const std::array<std::optional<std::size_t>, 24>& parents,
                                                  const std::array<Transform, 24>& part_frames) {
        for (std::size_t part = 0; part < 24; ++part) {
            std::optional<std::size_t> parent = parents[part];
            std::array<bool, 24> visited{};
            visited[part] = true;
            while (parent) {
                const std::size_t index = *parent;
                if (index >= 24 || visited[index]) {
                    return rs::make_err("Invalid physical ancestor chain for IK part " +
                                        std::to_string(part));
                }
                visited[index] = true;
                parent = parents[index];
            }
        }
        Geometry out;
        out.parents = parents;
        for (std::size_t i = 0; i < 24; ++i) {
            out.inverse_part_frames[i] = math::inverse_affine(part_frames[i]);
        }
        return out;
    }

    rs::Result<rs::Unit, std::string> validate_limbs(const std::array<LimbBinding, 4>& bindings) const {
        for (const LimbBinding& binding : bindings) {
            const std::size_t end = binding.parent_part.value_or(binding.part);
            auto missing_chain = [&] {
                const auto p = parents[end];
                return !p || !parents.at(*p);
            };
            if (binding.part >= 24 || end >= 24 || missing_chain()) {
                return rs::make_err("IK part " + std::to_string(end) +
                                    " has no two-bone physical ancestor chain");
            }
        }
        return rs::Unit{};
    }
};

std::array<bool, 4> update(const std::array<status::LimbStatus, 4>& statuses, std::array<LimbFrames, 4>& frames,
                           const std::array<LimbBinding, 4>& bindings, const Geometry& geometry,
                           const std::array<Transform, 24>& original_animation, std::array<Transform, 24>& drives,
                           const Transform& world_to_animation, two_bone::AngleLimits limits);

///82BF1F20, two82BF1D28 frames and target * rigidInverse(original). (pub(super))
std::optional<Transform> line_mapping(Vector start, Vector end, Vector normal, Vector new_start, Vector new_end,
                                      Vector new_normal);

/// (pub(super))
Vector normalize_safe(Vector value);

}  // namespace s3::animation::foot_ik::drive
