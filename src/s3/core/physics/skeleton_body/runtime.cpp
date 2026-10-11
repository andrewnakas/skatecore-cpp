// Port of skate-core/src/physics/skeleton_body/runtime.rs.
#include "s3/core/physics/board_pose.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::physics::skeleton_body::runtime {

using board_pose::orthonormalize_part_basis;
using board_pose::part_transform;
using board_pose::PartPose;
using board_pose::PoseMatrix;
using board_runtime::BoardMotion;
using board_runtime::initialized_body;
using math::Basis3;
using math::Vector3;
using rigid_body::RetailBodyRates;
using rigid_body::RetailInertiaDynamics;
using rigid_body::RetailLocalMassFrame;
using rigid_body::RetailQuaternion;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::IDENTITY;
using skeleton_root::inverse_rigid;

namespace {

PoseMatrix words(const AnimationPartTransform& frame) {
    PoseMatrix out{};
    for (std::size_t i = 0; i < 16; ++i) out[i] = rs::to_bits(frame[i / 4][i % 4]);
    return out;
}
AnimationPartTransform floats(const PoseMatrix& frame) {
    AnimationPartTransform out{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) out[i][j] = rs::from_bits(frame[i * 4 + j]);
    return out;
}
PoseMatrix mass_words(const RetailLocalMassFrame& frame) {
    AnimationPartTransform m = IDENTITY;
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t j = 0; j < 3; ++j) m[column][j] = frame.basis.columns[column][j];
    }
    m[3] = {frame.translation.x, frame.translation.y, frame.translation.z, 0.0f};
    return words(m);
}
std::array<std::uint32_t, 10> inertia_words(const RetailInertiaDynamics& d) {
    return rs::to_bits(std::array<float, 10>{
        d.inverse_tensor.x,
        d.inverse_tensor.y,
        d.inverse_tensor.z,
        0.0f,
        d.inverse_mass,
        d.spherical,
        d.maximum_linear_velocity,
        d.maximum_angular_velocity,
        d.linear_drag,
        d.angular_drag,
    });
}
std::array<std::uint32_t, 44> body_words(const RetailBodyRates& rates) {
    std::array<std::uint32_t, 44> w{};
    const RetailQuaternion q = rates.orientation;
    w[0] = rs::to_bits(q.x);
    w[1] = rs::to_bits(q.y);
    w[2] = rs::to_bits(q.z);
    w[3] = rs::to_bits(q.w);
    w[4] = rs::to_bits(rates.position.x);
    w[5] = rs::to_bits(rates.position.y);
    w[6] = rs::to_bits(rates.position.z);
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) w[16 + i * 4 + j] = rs::to_bits(rates.basis.columns[i][j]);
    }
    return w;
}

}  // namespace

SkeletonBody SkeletonBody::new_(SkeletonBodyDefinition definition,
                                const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& authored_in,
                                AnimationPartTransform spawn, RetailSimulationStep simulation) {
    // GetPartFrame82BD9878 normalizes the mapped animation frame before
    // Reset composes the world alignment. PartSetTransform normalizes
    // again after applying the local mass frame; the two stages differ.
    std::array<AnimationPartTransform, ANIMATION_PART_COUNT> authored{};
    for (std::size_t i = 0; i < ANIMATION_PART_COUNT; ++i) {
        authored[i] = floats(orthonormalize_part_basis(words(authored_in[i])));
    }
    const AnimationPartTransform animation_to_world = compose_affine(spawn, inverse_rigid(authored[0]));
    SkeletonBody result;
    for (std::size_t i = 0; i < PART_COUNT; ++i) {
        const AnimationPartTransform desired =
            i < ANIMATION_PART_COUNT ? compose_affine(animation_to_world, authored[i]) : spawn;
        const auto mass = definition.parts[i].animated;
        RetailInertiaDynamics inertia = mass.dynamics;
        // SetUpNormal82BE7280 loads the independently cached inverse mass
        // for the 23 skater bones. Root/extra bodies retain their setup.
        if (i >= 1 && i < ANIMATION_PART_COUNT) {
            inertia.inverse_mass = definition.parts[i].inverse_mass_animated;
        }
        PartPose part{words(desired), mass_words(mass.local_mass_frame), std::array<std::uint32_t, 44>{},
                      inertia_words(inertia)};
        board_pose::set_part_transform(part, words(desired));
        result.bodies_[i] = initialized_body(part, inertia, simulation, BoardMotion::Active);
    }
    result.definition = definition;
    result.animation_to_world = animation_to_world;
    result.record = SkeletonPhysicalRecord::default_();
    // SkeletonInit82BD8010..804C makes two real physical observations.
    // The second eliminates the constructor's zero-position derivative.
    const auto pose = result.part_transforms();
    result.record.update(pose, pose[0], result.definition.animation_masses.fractional);
    result.record.update(pose, pose[0], result.definition.animation_masses.fractional);
    return result;
}

void SkeletonBody::set_part_transform(std::size_t part, AnimationPartTransform frame) {
    BodySnapshot& body = bodies_[part];
    PartPose pose{words(frame), mass_words(definition.parts[part].animated.local_mass_frame),
                  body_words(body.rates), inertia_words(body.inertia)};
    board_pose::set_part_transform(pose, words(frame));
    const auto packed = rs::unwrap(pose.body);
    auto f = [&](std::size_t i) { return rs::from_bits(packed[i]); };
    body.rates.orientation = RetailQuaternion{f(0), f(1), f(2), f(3)};
    body.rates.position = Vector3{f(4), f(5), f(6)};
    body.rates.basis = Basis3{{{
        {f(16), f(17), f(18)},
        {f(20), f(21), f(22)},
        {f(24), f(25), f(26)},
    }}};
    body.rates.world_inverse_inertia = Basis3{{{
        {f(28), f(29), f(30)},
        {f(29), f(33), f(34)},
        {f(30), f(34), f(32)},
    }}};
    record.pose[part] = frame;
}

std::array<AnimationPartTransform, PART_COUNT> SkeletonBody::part_transforms() const {
    std::array<AnimationPartTransform, PART_COUNT> out{};
    for (std::size_t i = 0; i < PART_COUNT; ++i) {
        const PartPose part{words(IDENTITY), mass_words(definition.parts[i].animated.local_mass_frame),
                            body_words(bodies_[i].rates), std::nullopt};
        out[i] = floats(part_transform(part));
    }
    return out;
}

void SkeletonBody::publish_physical_record(AnimationPartTransform board) {
    const auto pose = part_transforms();
    record.update(pose, board, definition.animation_masses.fractional);
}

void SkeletonBody::apply_part_displacement(std::size_t part, std::array<float, 4> displacement) {
    const float step = rs::from_bits(0x3C888889u);
    float inverse_step = native_arithmetic::reciprocal_estimate(step);
    for (int i = 0; i < 2; ++i) {
        inverse_step = rs::mul_add(inverse_step, rs::mul_add(-inverse_step, step, 1.0f), inverse_step);
    }
    BodySnapshot& body = bodies_[part];
    std::array<float, 4> value{};
    for (std::size_t i = 0; i < 4; ++i) value[i] = (displacement[i] * inverse_step) * body.inertia.inverse_mass;
    body.rates.force_acceleration.x += value[0];
    body.rates.force_acceleration.y += value[1];
    body.rates.force_acceleration.z += value[2];
    body.rates.cool_down = 0;
}

}  // namespace s3::physics::skeleton_body::runtime
