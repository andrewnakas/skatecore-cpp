// Port of skate-core/src/physics/skeleton_body/targets.rs.
#include "s3/core/physics/board_pose.hpp"
#include "s3/core/physics/board_runtime.hpp"
#include "s3/core/physics/skeleton_body.hpp"

namespace s3::physics::skeleton_body::targets {

using board_pose::orthonormalize_rotation;
using board_pose::PartPose;
using board_pose::set_part_transform;
using board_runtime::BoardMotion;
using board_runtime::initialized_body;
using drive_parameters::RetailDriveType;
using math::Basis3;
using math::Vector3;
using physics::drive_frames::retail_quaternion_from_basis;
using physics::drive_frames::RetailDriveFrame;
using rigid_body::RetailInertiaDynamics;
using rigid_body::RetailQuaternion;
using skeleton_animation_record::IDENTITY;

namespace {

const RetailInertiaDynamics STATIC_INERTIA{Vector3{0.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

std::array<std::uint32_t, 16> words(const AnimationPartTransform& frame) {
    std::array<std::uint32_t, 16> out{};
    for (std::size_t i = 0; i < 16; ++i) out[i] = rs::to_bits(frame[i / 4][i % 4]);
    return out;
}
AnimationPartTransform floats(const std::array<std::uint32_t, 16>& frame) {
    AnimationPartTransform out{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j) out[i][j] = rs::from_bits(frame[i * 4 + j]);
    return out;
}

}  // namespace

RetailDriveParams hard(float velocity, float strength) {
    return RetailDriveParams{velocity, 0.0f, strength, RetailDriveType::HardDrive};
}

SkeletonTargets SkeletonTargets::new_(RetailSimulationStep simulation) {
    SkeletonTargets out;
    for (std::size_t i = 0; i < TARGET_COUNT; ++i) {
        PartPose part{words(IDENTITY), words(IDENTITY), std::array<std::uint32_t, 44>{}, std::nullopt};
        set_part_transform(part, words(IDENTITY));
        // Original82BEAED4 inserts STATIC_BODY(1), giving zero inverse
        // mass/tensor/rates. The centered.1m box's authored100kg and its
        // disabled collision volume never participate in that response.
        out.bodies[i] = initialized_body(part, STATIC_INERTIA, simulation, BoardMotion::Static);
    }
    const RetailDriveFrame identity{RetailQuaternion::IDENTITY, Vector3::ZERO};
    //82BEAD34..ADC8: X=(0,1,0),Y=(0,0,1),Z=(1,0,0).
    RetailDriveFrame extra = identity;
    extra.orientation = retail_quaternion_from_basis(
        Basis3{{{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}}}});
    for (std::size_t i = 0; i < TARGET_COUNT; ++i) {
        out.frames[i] = RetailDriveFrames{i >= 2 ? extra : identity, identity};
    }
    const RetailDriveParams strong = hard(HARD_VELOCITY, HARD_STRENGTH);
    const RetailDriveParams root = hard(rs::from_bits(0x426FFFFFu), HARD_STRENGTH);
    out.dynamics = {
        RetailDriveDynamics{strong, strong},
        RetailDriveDynamics{root, root},
        RetailDriveDynamics{hard(0.0f, 0.0f), strong},
        RetailDriveDynamics{hard(0.0f, 0.0f), strong},
    };
    return out;
}

void SkeletonTargets::reset(AnimationPartTransform hips, AnimationPartTransform spawn) {
    const AnimationPartTransform frames[2] = {hips, spawn};
    for (std::size_t index = 0; index < 2; ++index) {
        set_transform(index, floats(orthonormalize_rotation(words(frames[index]))));
    }
}

void SkeletonTargets::set_transform(std::size_t index, AnimationPartTransform frame) {
    BodySnapshot& body = bodies[index];
    PartPose part{words(frame), words(IDENTITY), std::array<std::uint32_t, 44>{}, std::nullopt};
    set_part_transform(part, words(frame));
    if (!part.body) S3_PANIC("Target has an actual rigid body");
    const auto w = *part.body;
    auto f = [&](std::size_t i) { return rs::from_bits(w[i]); };
    body.rates.orientation = RetailQuaternion{f(0), f(1), f(2), f(3)};
    body.rates.position = Vector3{f(4), f(5), f(6)};
    for (std::size_t i = 0; i < 3; ++i) {
        body.rates.basis.columns[i] = {f(16 + i * 4), f(17 + i * 4), f(18 + i * 4)};
    }
}

AnimationPartTransform SkeletonTargets::transform(std::size_t index) const {
    const BodySnapshot body = bodies[index];
    AnimationPartTransform m = IDENTITY;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) m[i][j] = body.rates.basis.columns[i][j];
    }
    const Vector3 p = body.rates.position;
    m[3] = {p.x, p.y, p.z, 0.0f};
    return m;
}

void SkeletonTargets::apply_future_deck_displacement(Vector3 displacement) {
    AnimationPartTransform frame = transform(0);
    frame[3][0] += displacement.x;
    frame[3][1] += displacement.y;
    frame[3][2] += displacement.z;
    set_transform(0, frame);
}

}  // namespace s3::physics::skeleton_body::targets
