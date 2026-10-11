// Port of skate-core/src/animation/foot_ik/external.rs.
#include "s3/core/animation/foot_ik/external.hpp"

#include "s3/core/animation/foot_ik/math.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::animation::foot_ik::external {

using math::axis_rotation;
using math::cross;
using math::length;
using math::limit_length;
using math::reciprocal;
using physics::skeleton_animation_record::compose_affine;
using physics::skeleton_animation_record::IDENTITY;
using physics::skeleton_animation_record::transform_point;
using status::Mode;
using transforms::inverse_rigid;

void update(ExternalTarget& target, status::LimbStatus& status, transforms::LimbFrames& frames,
            transforms::LimbBinding binding, const std::array<Transform, 24>& animation,
            const Transform& animation_to_world, const Transform& world_to_animation) {
    const Transform& animated = animation[binding.part];
    const Transform original = compose_affine(animation_to_world, animated);
    Transform rotation = IDENTITY;
    if (target.normal_blend > 0.0f) {
        // Native v16 is the fixed Y basis82139A20, not the animated limb's up.
        const math::Vector axis = cross({0.0f, 1.0f, 0.0f, 0.0f}, target.normal);
        const float magnitude = length(axis);
        if (magnitude > 0.05f) {
            const float inverse = reciprocal(magnitude, 2);
            const math::Vector scaled{inverse * axis[0], inverse * axis[1], inverse * axis[2], inverse * axis[3]};
            float angle = trigonometry::asin(magnitude);
            angle = -0.71f - angle >= 0.0f ? -0.71f : angle;
            angle = 0.71f - angle >= 0.0f ? angle : 0.71f;
            rotation = axis_rotation(scaled, angle * target.normal_blend);
        }
        if (!target.normal_set) {
            const float next = target.normal_blend - 0.2f;
            target.normal_blend = -next >= 0.0f ? 0.0f : next;
        }
    }
    if (status.mode == Mode::Local) {
        const float weight = status.target_blend;
        std::array<float, 4> desired{};
        for (std::size_t lane = 0; lane < 4; ++lane) {
            desired[lane] = rs::mul_add(target.animation_position[lane] - animated[3][lane], weight,
                                        status.external_target_local_delta[lane] * (1.0f - weight));
        }
        std::array<float, 4> difference{};
        for (std::size_t lane = 0; lane < 4; ++lane) {
            difference[lane] = desired[lane] - status.external_target_local_delta[lane];
        }
        const std::array<float, 4> change = limit_length(difference, 0.025f);
        for (std::size_t lane = 0; lane < 4; ++lane) {
            status.external_target_local_delta[lane] += change[lane];
            // The source publishes desired before its history limit. Do not
            // replace this with the separately limited retained delta.
            target.animation_position[lane] = animated[3][lane] + desired[lane];
        }
        target.world_position = transform_point(animation_to_world, target.animation_position);
    } else {
        target.animation_position = transform_point(world_to_animation, target.world_position);
    }
    std::array<float, 4> raw_offset{};
    for (std::size_t lane = 0; lane < 4; ++lane) raw_offset[lane] = target.world_position[lane] - original[3][lane];
    const std::array<float, 4> offset = limit_length(raw_offset, 0.7f);
    Transform from_origin = IDENTITY;
    for (std::size_t lane = 0; lane < 4; ++lane) {
        from_origin[3][lane] = rs::from_bits(rs::to_bits(original[3][lane]) ^ 0x80000000u);
    }
    const Transform rotated = compose_affine(rotation, from_origin);
    Transform to_origin = IDENTITY;
    for (std::size_t lane = 0; lane < 4; ++lane) to_origin[3][lane] = original[3][lane] + offset[lane];
    const Transform adjustment = compose_affine(to_origin, rotated);
    frames.external_world = compose_affine(adjustment, original);
    if (binding.parent_part) {
        const Transform relative = compose_affine(inverse_rigid(animated), animation[*binding.parent_part]);
        frames.external_parent_world = compose_affine(frames.external_world, relative);
    }
}

}  // namespace s3::animation::foot_ik::external
