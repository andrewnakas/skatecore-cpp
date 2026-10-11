// Port of skate-core/src/animation/foot_ik/physical_solve.rs.
#include "s3/core/animation/foot_ik/physical_solve.hpp"

#include "s3/core/animation/foot_ik/two_bone.hpp"

namespace s3::animation::foot_ik::physical_solve {

using drive::line_mapping;
using drive::normalize_safe;
using math::cross;
using math::Vector;
using physics::skeleton_animation_record::compose_affine;
using physics::skeleton_animation_record::transform_point;
using two_bone::AngleLimits;
using two_bone::SolveResult;

namespace {
Vector sub(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
}  // namespace

bool solve(physics::skeleton_body::SkeletonBody& body, const drive::Geometry& geometry, std::size_t end_part,
           std::array<float, 4> offset) {
    const auto middle_opt = geometry.parents[end_part];
    if (!middle_opt) return false;
    const std::size_t middle_part = *middle_opt;
    const auto root_opt = geometry.parents[middle_part];
    if (!root_opt) return false;
    const std::size_t root_part = *root_opt;
    auto point = [&](std::size_t part) {
        return transform_point(body.record.pose[part], geometry.inverse_part_frames[part][3]);
    };
    const Vector root = point(root_part);
    const Vector middle = point(middle_part);
    const Vector end = point(end_part);
    Vector target{};
    for (std::size_t i = 0; i < 4; ++i) target[i] = end[i] + offset[i];
    Vector solved_middle{};
    //Literal0 and180, no original-angle override and recursion budget1.
    //The source requires return0; its extended/invalid paths BOTH reject.
    if (two_bone::solve(root, middle, end, solved_middle, target, AngleLimits{0.0f, 180.0f}, false, 1) !=
        SolveResult::Solved) {
        return false;
    }
    const Vector original_normal = cross(sub(root, middle), sub(end, middle));
    const Vector solved_normal = cross(sub(root, solved_middle), sub(target, solved_middle));
    Vector summed{};
    for (std::size_t i = 0; i < 4; ++i) summed[i] = original_normal[i] + solved_normal[i];
    const Vector normal = normalize_safe(summed);
    const auto mid_map = line_mapping(middle, end, normal, solved_middle, target, normal);
    if (!mid_map) return false;
    const auto root_map = line_mapping(root, middle, normal, root, solved_middle, normal);
    if (!root_map) return false;
    const auto middle_frame = compose_affine(*mid_map, body.record.pose[middle_part]);
    const auto root_frame = compose_affine(*root_map, body.record.pose[root_part]);
    //Unlike pre-physics drive solving, no extra orthonormalization precedes
    //the setter. The live body normalizes; the record keeps these frames.
    body.set_part_transform(middle_part, middle_frame);
    body.set_part_transform(root_part, root_frame);
    return true;
}

}  // namespace s3::animation::foot_ik::physical_solve
