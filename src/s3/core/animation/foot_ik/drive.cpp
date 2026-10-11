// Port of skate-core/src/animation/foot_ik/drive.rs (update and its solve/mapping chain).
#include "s3/core/animation/foot_ik/drive.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::animation::foot_ik::drive {

using math::cross;
using math::length;
using math::normalize;
using physics::native_arithmetic::dot3;
using physics::skeleton_animation_record::compose_affine;
using physics::skeleton_animation_record::transform_point;
using physics::skeleton_root::orthonormalize;
using rs::from_bits;
using status::LimbStatus;
using status::Mode;
using two_bone::AngleLimits;
using two_bone::SolveResult;

namespace {

Vector subtract(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
void add_translation(Transform& frame, Vector offset) {
    for (std::size_t lane = 0; lane < 4; ++lane) frame[3][lane] += offset[lane];
}

///82BD41B0 clamps a point relative to the original chain root using scalar
///division after the two-refinement length; it is not limit_length's reciprocal.
Vector clamp_point(Vector point, Vector origin, float maximum) {
    const Vector delta = subtract(point, origin);
    const float distance = length(delta);
    if (maximum >= distance) return point;
    const float scale = maximum / distance;
    Vector out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(delta[i], scale, origin[i]);
    return out;
}

std::optional<Transform> line_frame(Vector start, Vector end, Vector normal) {
    const Vector line = subtract(end, start);
    const Vector across = cross(line, normal);
    const Vector third = cross(across, line);
    const Vector x = normalize_safe(across);
    const Vector z = normalize_safe(third);
    const Vector y = normalize_safe(line);
    const float product = (dot3(z, z) * dot3(x, x)) * dot3(y, y);
    //830BD300 <-821647E0, initializer82F82690.
    if (product > from_bits(0x37800000u)) {
        return Transform{{x, y, z, start}};
    }
    return std::nullopt;
}

///82BF0FA8, using original animation joint centers and mutable volume frames.
std::optional<Vector> solve_drives(std::size_t end_part, Vector offset, float maximum_distance,
                                   const Geometry& geometry, const std::array<Transform, 24>& original,
                                   std::array<Transform, 24>& drives, AngleLimits limits) {
    const auto middle_opt = geometry.parents[end_part];
    if (!middle_opt) return std::nullopt;
    const std::size_t middle_part = *middle_opt;
    const auto root_opt = geometry.parents[middle_part];
    if (!root_opt) return std::nullopt;
    const std::size_t root_part = *root_opt;
    const Vector root = original[root_part][3];
    const Vector middle = original[middle_part][3];
    const Vector end = original[end_part][3];
    Vector requested{};
    for (std::size_t i = 0; i < 4; ++i) requested[i] = end[i] + offset[i];
    Vector target = clamp_point(requested, root, maximum_distance);
    Vector solved_middle{};
    if (two_bone::solve(root, middle, end, solved_middle, target, limits, true, 2) == SolveResult::Invalid) {
        return std::nullopt;
    }
    const Vector residual = subtract(target, requested);
    const Vector original_normal = cross(subtract(root, middle), subtract(end, middle));
    const Vector solved_normal = cross(subtract(root, solved_middle), subtract(target, solved_middle));
    Vector summed{};
    for (std::size_t i = 0; i < 4; ++i) summed[i] = original_normal[i] + solved_normal[i];
    const Vector normal = normalize_safe(summed);
    const auto middle_mapping = line_mapping(middle, end, normal, solved_middle, target, normal);
    if (!middle_mapping) return std::nullopt;
    const auto root_mapping = line_mapping(root, middle, normal, root, solved_middle, normal);
    if (!root_mapping) return std::nullopt;
    drives[root_part] = orthonormalize(compose_affine(*root_mapping, drives[root_part]));
    drives[middle_part] = orthonormalize(compose_affine(*middle_mapping, drives[middle_part]));
    return residual;
}

}  // namespace

std::array<bool, 4> update(const std::array<LimbStatus, 4>& statuses, std::array<LimbFrames, 4>& frames,
                           const std::array<LimbBinding, 4>& bindings, const Geometry& geometry,
                           const std::array<Transform, 24>& original_animation, std::array<Transform, 24>& drives,
                           const Transform& world_to_animation, AngleLimits limits) {
    std::array<bool, 4> updated{};
    for (std::size_t limb = 0; limb < 4; ++limb) {
        if (statuses[limb].mode == Mode::Disabled) continue;
        const LimbBinding binding = bindings[limb];
        const Transform target = compose_affine(world_to_animation, frames[limb].world);
        std::optional<Transform> parent_target;
        if (binding.parent_part) parent_target = compose_affine(world_to_animation, frames[limb].parent_world);
        const std::size_t driven = binding.parent_part.value_or(binding.part);
        const Transform& driven_target = parent_target ? *parent_target : target;
        const Vector endpoint = transform_point(driven_target, geometry.inverse_part_frames[driven][3]);
        const Vector offset = subtract(endpoint, original_animation[driven][3]);
        //82BF01DC..1F4: these are maximum root-to-target distances passed
        //through f1 into82BD41B0, not animation blend weights.
        const float maximum_distance = limb < 2 ? from_bits(0x3F4CCCCDu) : 1.0f;
        if (const auto residual =
                solve_drives(driven, offset, maximum_distance, geometry, original_animation, drives, limits)) {
            drives[binding.part] = target;
            add_translation(drives[binding.part], *residual);
            // The source also adds the same returned delta to retained world
            //targets. Do not insert an extra rotation absent from82BF0458.
            add_translation(frames[limb].world, *residual);
            if (binding.parent_part) {
                const std::size_t parent = *binding.parent_part;
                drives[parent] = *parent_target;
                add_translation(drives[parent], *residual);
                add_translation(frames[limb].parent_world, *residual);
            }
            updated[limb] = true;
        }
    }
    return updated;
}

std::optional<Transform> line_mapping(Vector start, Vector end, Vector normal, Vector new_start, Vector new_end,
                                      Vector new_normal) {
    const auto original = line_frame(start, end, normal);
    if (!original) return std::nullopt;
    const auto target = line_frame(new_start, new_end, new_normal);
    if (!target) return std::nullopt;
    return compose_affine(*target, transforms::inverse_rigid(*original));
}

Vector normalize_safe(Vector value) {
    const float magnitude = length(value);
    const Vector normalized = normalize(value);
    //830BD350 <-82181A88; no arbitrary fallback axis for a degenerate line.
    if (magnitude > from_bits(0x358637BDu)) return normalized;
    return Vector{0.0f, 0.0f, 0.0f, 0.0f};
}

}  // namespace s3::animation::foot_ik::drive
