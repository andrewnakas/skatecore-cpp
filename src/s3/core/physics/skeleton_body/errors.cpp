// Port of skate-core/src/physics/skeleton_body/errors.rs.
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::errors {

using namespace collision_vector;

namespace {
constexpr std::size_t CANDIDATE_PARTS[8] = {23, 17, 18, 16, 21, 22, 20, 1};
}  // namespace

void SkeletonPoseErrors::reset_history() {
    targets = {V{0.0f, 0.0f, 0.0f, 0.0f}, V{0.0f, 0.0f, 0.0f, 0.0f}, V{0.0f, 0.0f, 0.0f, 0.0f}};
}

void SkeletonPoseErrors::set_targets(ExtraTargetPositions positions) {
    targets = {positions.com, positions.lifted_com, positions.following_com};
}

void SkeletonPoseErrors::update(const SkeletonPhysicalRecord& physical, AnimationPartTransform animation_to_world,
                                const std::array<AnimationPartTransform, 24>& drive_frames) {
    parts[0] = V{0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t part = 1; part < 24; ++part) {
        parts[part] = sub(physical.pose[part][3], transform(animation_to_world, drive_frames[part][3]));
    }
    for (std::size_t i = 0; i < 2; ++i) {
        extra[i] = sub(physical.pose[24 + i][3], targets[1 + i]);
    }
}

SkeletonNormalError SkeletonPoseErrors::normal_response(const SkeletonCollisionFeedback& collision,
                                                        V support_normal) const {
    std::array<V, 8> filtered{};
    for (std::size_t i = 0; i < 8; ++i) {
        filtered[i] = collision.filter_error(parts[CANDIDATE_PARTS[i]], support_normal);
    }
    V impulse = filtered[0];
    for (std::size_t i = 1; i < 8; ++i) {
        if (dot(filtered[i], filtered[i]) > dot(impulse, impulse)) {
            impulse = filtered[i];
        }
    }
    return SkeletonNormalError{impulse, {V{0.0f, 0.0f, 0.0f, 0.0f}, V{0.0f, 0.0f, 0.0f, 0.0f}}, maximum_error()};
}

SkeletonNormalError SkeletonPoseErrors::partial_response() const {
    return SkeletonNormalError{extra[0], extra, maximum_error()};
}

float SkeletonPoseErrors::maximum_error() const {
    std::array<V, 8> candidates{};
    for (std::size_t i = 0; i < 8; ++i) candidates[i] = parts[CANDIDATE_PARTS[i]];
    V maximum = dot(candidates[0], candidates[0]) > dot(candidates[1], candidates[1]) ? candidates[0] : candidates[1];
    for (std::size_t i = 2; i < 8; ++i) {
        if (dot(candidates[i], candidates[i]) > dot(maximum, maximum)) {
            maximum = candidates[i];
        }
    }
    return length(maximum);
}

}  // namespace s3::physics::skeleton_body::errors
