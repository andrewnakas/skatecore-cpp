// Port of skate-core/src/physics/skeleton_body/record.rs.
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::record {

void SkeletonPhysicalRecord::reset(const std::array<AnimationPartTransform, PART_COUNT>& parts) {
    pose = parts;
    for (std::size_t i = 0; i < PART_COUNT; ++i) {
        positions[i] = parts[i][3];
        velocities[i] = {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

void SkeletonPhysicalRecord::update(const std::array<AnimationPartTransform, PART_COUNT>& parts,
                                    AnimationPartTransform board_frame,
                                    const std::array<float, ANIMATION_PART_COUNT>& fractional) {
    timestep = rs::from_bits(0x3C888889u);
    for (std::size_t i = 0; i < PART_COUNT; ++i) {
        const AnimationPartTransform transform = i == 0 ? board_frame : parts[i];
        pose[i] = transform;
        for (std::size_t lane = 0; lane < 4; ++lane) {
            const float velocity =
                (transform[3][lane] - positions[i][lane]) * rs::from_bits(0x426FFFFFu);
            velocity_changes[i][lane] = velocity - velocities[i][lane];
            velocities[i][lane] = velocity;
        }
        positions[i] = transform[3];
    }
    std::array<float, 4> position{};
    std::array<float, 4> velocity{};
    for (std::size_t i = 0; i < ANIMATION_PART_COUNT; ++i) {
        const float weight = fractional[i];
        for (std::size_t lane = 0; lane < 4; ++lane) {
            position[lane] = rs::mul_add(pose[i][3][lane], weight, position[lane]);
            velocity[lane] = rs::mul_add(velocities[i][lane], weight, velocity[lane]);
        }
    }
    centre_of_mass = position;
    centre_of_mass_velocity = velocity;
}

}  // namespace s3::physics::skeleton_body::record
