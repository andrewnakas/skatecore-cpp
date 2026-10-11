// Port of skate-core/src/physics/skeleton_body/joints.rs.
#include "s3/core/physics/constraint_batch.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/joint_builder.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/skeleton_root.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_body::joints {

using constraint_batch::compile_active;
using joint_builder::build_retail_joint_jacobian;
using joint_builder::RetailJointBodyInput;
using joint_builder::RetailJointBuildInput;
using math::Basis3;
using physics::drive_frames::retail_quaternion_from_basis;
using rigid_body::pack_world_inverse_inertia;
using skeleton_animation_record::compose_affine;
using skeleton_animation_record::physics_bone_frame;
using skeleton_root::inverse_rigid;

namespace {

float inverse_native_step() {
    const float step = rs::from_bits(0x3C888889u);  //820849C8
    float reciprocal = native_arithmetic::reciprocal_estimate(step);
    for (int i = 0; i < 2; ++i) {
        reciprocal = rs::mul_add(reciprocal, rs::mul_add(-reciprocal, step, 1.0f), reciprocal);
    }
    return reciprocal;
}
float minimum_angle(float value) {
    const float minimum = rs::from_bits(0x3C23D70Au);
    return value - minimum >= 0.0f ? value : minimum;
}
/// `words` is the 8-word sub-slice starting at `offset`.
void pack_frame(std::array<std::uint32_t, 20>& words, std::size_t offset, const AnimationPartTransform& frame,
                const std::array<float, 4>& anchor) {
    Basis3 basis;
    for (std::size_t i = 0; i < 3; ++i) basis.columns[i] = {frame[i][0], frame[i][1], frame[i][2]};
    const auto q = retail_quaternion_from_basis(basis);
    const float values[4] = {q.x, q.y, q.z, q.w};
    for (std::size_t i = 0; i < 4; ++i) words[offset + i] = rs::to_bits(values[i]);
    for (std::size_t i = 0; i < 4; ++i) words[offset + 4 + i] = rs::to_bits(anchor[i]);
}
RetailJointBodyInput joint_body(const BodySnapshot& body) {
    const auto& rates = body.rates;
    RetailJointBodyInput out;
    out.reaction_guest_address = 0;
    out.state = body.state_flags;
    out.orientation = rates.orientation;
    out.center_of_mass = rates.position;
    out.basis = rates.basis;
    out.linear_velocity = rates.linear_velocity;
    out.angular_velocity = rates.angular_velocity;
    out.force_acceleration = rates.force_acceleration;
    out.torque_acceleration = rates.torque_acceleration;
    out.inverse_mass = body.inertia.inverse_mass;
    out.world_inverse_inertia = pack_world_inverse_inertia(rates.world_inverse_inertia);
    return out;
}

}  // namespace

rs::Result<SkeletonJoints, const char*> SkeletonJoints::new_(
    const std::array<AnimationPartTransform, ANIMATION_PART_COUNT>& initial_bones,
    const std::array<std::optional<std::size_t>, ANIMATION_PART_COUNT>& parents,
    const std::array<JointBone, ANIMATION_PART_COUNT>& bones, const std::array<JointSettings, JOINT_COUNT>& settings,
    SkeletonJointSettings global) {
    std::vector<SkeletonJoint> records;
    records.reserve(JOINT_COUNT);
    for (std::size_t child = 0; child < ANIMATION_PART_COUNT; ++child) {
        if (!parents[child]) {
            continue;
        }
        const std::size_t parent = *parents[child];
        if (parent >= ANIMATION_PART_COUNT || parent == child) {
            return rs::make_err("Invalid physical skeleton joint parent");
        }
        if (records.size() >= settings.size()) {
            return rs::make_err("Too many physical skeleton joints");
        }
        const JointSettings setting = settings[records.size()];
        const JointBone bone = bones[child];
        const AnimationPartTransform inverse_child_volume = inverse_rigid(bone.volume_frame);
        const AnimationPartTransform inverse_parent_volume = inverse_rigid(bones[parent].volume_frame);
        const AnimationPartTransform parent_orientation =
            physics_bone_frame(bone.parent_orientation, {0.0f, 0.0f, 0.0f, 0.0f});
        const AnimationPartTransform joint_orientation =
            physics_bone_frame(bone.joint_orientation, {0.0f, 0.0f, 0.0f, 0.0f});
        const AnimationPartTransform parent_joint =
            compose_affine(inverse_parent_volume, inverse_rigid(parent_orientation));
        const AnimationPartTransform relative_bones =
            compose_affine(inverse_rigid(initial_bones[parent]), initial_bones[child]);
        const std::array<float, 4> parent_anchor = compose_affine(inverse_parent_volume, relative_bones)[3];
        const AnimationPartTransform child_frame = compose_affine(inverse_child_volume, joint_orientation);
        const AnimationPartTransform parent_frame = compose_affine(parent_joint, joint_orientation);
        RetailJointFramesRaw frames{};
        pack_frame(frames.words, 0, child_frame, inverse_child_volume[3]);
        pack_frame(frames.words, 8, parent_frame, parent_anchor);
        // InitJoints passes the identity matrix to SetLinearFrame82BD3858.
        frames.words[19] = rs::to_bits(1.0f);

        RetailJointParametersRaw parameters{};
        const float frequency = inverse_native_step();
        for (std::size_t i = 0; i < 4; ++i) {
            parameters.words[4 + i] = rs::to_bits(global.displacement_limit[i] * frequency);
        }
        const float scalar_frequency = rs::from_bits(0x426FFFFFu);
        parameters.words[8] = rs::to_bits(global.twist_displacement_limit * scalar_frequency);
        parameters.words[9] = rs::to_bits(global.swing_displacement_limit * scalar_frequency);
        parameters.words[14] = (setting.ball_joint || global.enforce_swing_free) ? 4u : 1u;
        parameters.words[15] = (setting.ball_joint || global.enforce_twist_free) ? 2u : 1u;
        // SetUpNormal runs after InitJoints and writes limits even for a
        // free/ball joint. It does not replace the mode selectors.
        const float swing = minimum_angle(bone.swing_limit * setting.swing_angle);
        const float twist = minimum_angle(bone.twist_limit * setting.twist_angle);
        const float limits[4] = {swing, twist, trigonometry::cos(swing), trigonometry::cos(twist)};
        for (std::size_t i = 0; i < 4; ++i) parameters.words[10 + i] = rs::to_bits(limits[i]);
        records.push_back(SkeletonJoint{parent, child, parameters, frames});
    }
    if (records.size() != JOINT_COUNT) {
        return rs::make_err("Physical skeleton requires22 joints");
    }
    SkeletonJoints out;
    for (std::size_t i = 0; i < JOINT_COUNT; ++i) out.records[i] = records[i];
    return out;
}

std::vector<JointConstraint> SkeletonJoints::build(const std::array<BodySnapshot, PART_COUNT>& bodies,
                                                   std::size_t reaction_base, float time_step) const {
    return compile_active(
        records,
        [&](const SkeletonJoint& record) {
            return std::array<std::uint32_t, 2>{bodies[record.child].state_flags, bodies[record.parent].state_flags};
        },
        [&](const SkeletonJoint& record) {
            RetailJointBuildInput input;
            input.parameters = record.parameters;
            input.frames = record.frames;
            input.body_a = joint_body(bodies[record.child]);
            input.body_b = joint_body(bodies[record.parent]);
            input.time_step = time_step;
            input.joint_guest_address = 0;
            return JointConstraint{build_retail_joint_jacobian(input), reaction_base + record.child,
                                   reaction_base + record.parent};
        });
}

}  // namespace s3::physics::skeleton_body::joints
