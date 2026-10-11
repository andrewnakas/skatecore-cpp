// Port of skate-core/src/physics/skeleton_general.rs.
#include "s3/core/physics/skeleton_general.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::skeleton_general {

using math::Vector3;
using native_arithmetic::reciprocal_estimate;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::transform_point;

namespace {
void reset_rates(SkeletonBody& body, std::size_t part, RetailSimulationStep simulation) {
    auto& rates = body.bodies_mut()[part].rates;
    rates.linear_velocity = Vector3::ZERO;
    rates.angular_velocity = Vector3::ZERO;
    //Original Reset copies the Simulation+144 gravity vector into forceXYZ;
    //it does not clear force to zero or reset energy/cooldown.
    rates.force_acceleration = simulation.gravity_acceleration;
    rates.torque_acceleration = Vector3::ZERO;
}
}  // namespace

std::array<float, 4> root_velocity(std::array<float, 4>& previous, std::array<float, 4> current, float dt) {
    std::array<float, 4> difference{};
    for (std::size_t i = 0; i < 4; ++i) difference[i] = current[i] - previous[i];
    float inverse = reciprocal_estimate(dt);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, dt, 1.0f), inverse);
    }
    std::array<float, 4> velocity{};
    for (std::size_t i = 0; i < 4; ++i) velocity[i] = difference[i] * inverse;
    previous = current;
    return velocity;
}

void apply_gravity_displacement(SkeletonBody& body, float processed_gravity) {
    const float step = rs::from_bits(0x3C888889u);
    const std::array<float, 4> source{0.0f, -processed_gravity, 0.0f, 0.0f};
    std::array<float, 4> displacement{};
    for (std::size_t i = 0; i < 4; ++i) displacement[i] = ((source[i] * 0.5f) * step) * step;
    for (std::size_t part = 0; part < 24; ++part) {
        body.apply_part_displacement(part, displacement);
    }
}

ResetObservation reset_to_animation(SkeletonBody& body, SkeletonAnimationRecord& animation,
                                    const std::array<Transform, 24>& drive_pose, const Transform& animation_to_world,
                                    Transform com_frame, RetailSimulationStep simulation) {
    for (std::size_t part = 0; part < 24; ++part) {
        const Transform world = compose_affine(animation_to_world, drive_pose[part]);
        body.set_part_transform(part, world);
        reset_rates(body, part, simulation);
    }
    for (std::size_t part : {24, 25}) {
        body.set_part_transform(part, com_frame);
        reset_rates(body, part, simulation);
    }
    const std::array<float, 4> local_hips = drive_pose[23][3];
    const std::array<float, 4> world_hips = transform_point(animation_to_world, local_hips);
    //82BEBBB0 obtains all26 current GetPartTransform results before clearing
    //their position/velocity histories and the four animation COM histories.
    const auto current = body.part_transforms();
    body.record.reset(current);
    animation.reset_history();
    return ResetObservation{local_hips, world_hips};
}

}  // namespace s3::physics::skeleton_general
