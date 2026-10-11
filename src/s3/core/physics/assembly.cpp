// Port of skate-core/src/physics/assembly.rs.
#include "s3/core/physics/assembly.hpp"

#include "s3/core/physics/constraint_batch.hpp"
#include "s3/core/physics/drive_preparation.hpp"
#include "s3/core/physics/joint_builder.hpp"
#include "s3/core/physics/joint_records.hpp"
#include "s3/core/physics/truck_frames.hpp"

namespace s3::physics::assembly {

using board::index;
using constraint_batch::compile_active;
using drive_solver::RetailDriveBodyState;
using joint_builder::RetailJointBodyInput;
using joint_builder::RetailJointBuildInput;

namespace {

RetailDriveFrames hook_frames(const std::array<std::uint32_t, 16>& words) {
    auto f = [&](std::size_t i) { return rs::from_bits(words[i]); };
    auto frame = [&](std::size_t offset) {
        return drive_frames::RetailDriveFrame{
            rigid_body::RetailQuaternion{f(offset), f(offset + 1), f(offset + 2), f(offset + 3)},
            math::Vector3{f(offset + 4), f(offset + 5), f(offset + 6)},
        };
    };
    return RetailDriveFrames{frame(0), frame(8)};
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
    out.world_inverse_inertia = rigid_body::pack_world_inverse_inertia(rates.world_inverse_inertia);
    return out;
}

RetailDriveBodyState drive_body(const BodySnapshot& body, std::size_t reaction_index) {
    const auto& rates = body.rates;
    RetailDriveBodyState out;
    out.reaction_index = reaction_index;
    out.state = body.state_flags;
    out.orientation = rates.orientation;
    out.basis = rates.basis;
    out.center_of_mass = rates.position;
    out.linear_velocity = rates.linear_velocity;
    out.angular_velocity = rates.angular_velocity;
    out.force_acceleration = rates.force_acceleration;
    out.torque_acceleration = rates.torque_acceleration;
    out.inverse_mass = body.inertia.inverse_mass;
    out.world_inverse_inertia = rigid_body::pack_world_inverse_inertia(rates.world_inverse_inertia);
    return out;
}

}  // namespace

BoardConstraints BoardConstraints::build(const std::array<BodySnapshot, BODY_COUNT>& bodies, const BoardHook& hook,
                                         std::array<RetailDriveFrames, 3> frames,
                                         RetailDriveDynamics truck_dynamics, float delta_seconds) {
    auto joints = compile_active(
        joint_records::default_joint_records(),
        [&](const joint_records::RetailJointRecord& record) {
            return std::array<std::uint32_t, 2>{bodies[index(record.live_body_a())].state_flags,
                                                bodies[index(record.live_body_b())].state_flags};
        },
        [&](const joint_records::RetailJointRecord& record) {
            const BodyId a = record.live_body_a();
            const BodyId b = record.live_body_b();
            RetailJointBuildInput input;
            input.parameters = record.parameters;
            input.frames = record.frames;
            input.body_a = joint_body(bodies[index(a)]);
            input.body_b = joint_body(bodies[index(b)]);
            input.time_step = delta_seconds;
            // Guest pointer metadata is not dereferenced by the host solver.
            input.joint_guest_address = 0;
            return JointConstraint{joint_builder::build_retail_joint_jacobian(input), index(a), index(b)};
        });
    const BodyId trucks[2] = {BodyId::FrontTruck, BodyId::BackTruck};
    const std::array<std::size_t, 2> truck_indices{0, 1};
    auto drives = compile_active(
        truck_indices,
        [&](std::size_t i) {
            const BodyId truck = trucks[i];
            return std::array<std::uint32_t, 2>{bodies[index(truck)].state_flags,
                                                bodies[index(BodyId::Deck)].state_flags};
        },
        [&](std::size_t i) {
            const BodyId truck = trucks[i];
            return drive_solver::build_drive_rows(drive_body(bodies[index(truck)], index(truck)),
                                                  drive_body(bodies[index(BodyId::Deck)], index(BodyId::Deck)),
                                                  frames[i], truck_dynamics, delta_seconds);
        });
    const std::array<int, 1> unit{0};
    const auto hook_drives = compile_active(
        unit,
        [&](int) {
            return std::array<std::uint32_t, 2>{hook.body.state_flags, bodies[index(BodyId::Deck)].state_flags};
        },
        [&](int) {
            return drive_solver::build_drive_rows(drive_body(hook.body, HOOK_REACTION),
                                                  drive_body(bodies[index(BodyId::Deck)], index(BodyId::Deck)),
                                                  frames[2], hook.drive.solver_dynamics(), delta_seconds);
        });
    drives.insert(drives.end(), hook_drives.begin(), hook_drives.end());
    return BoardConstraints{std::move(joints), std::move(drives)};
}

std::array<RetailDriveFrames, 3> prepare_drive_frames(std::array<RetailAffineTransform, 2> base,
                                                      std::array<float, 2> targets, BoardHook& hook) {
    const auto steering = truck_frames::steering_drive_frames(base, targets);
    std::array<RetailDriveFrames, 2> trucks{};
    for (std::size_t t = 0; t < 2; ++t) {
        std::array<std::uint32_t, 16> raw{};
        const drive_frames::RetailDriveFrame pair[2] = {steering[t].body_a, steering[t].body_b};
        for (std::size_t i = 0; i < 2; ++i) {
            const auto q = pair[i].orientation;
            const auto tr = pair[i].translation;
            const auto bits = rs::to_bits(std::array<float, 8>{q.x, q.y, q.z, q.w, tr.x, tr.y, tr.z, 0.0f});
            for (std::size_t k = 0; k < 8; ++k) raw[i * 8 + k] = bits[k];
        }
        drive_preparation::normalize_drive_frames(raw);
        trucks[t] = hook_frames(raw);
    }
    drive_preparation::normalize_drive_frames(hook.drive.frames);
    return {trucks[0], trucks[1], hook_frames(hook.drive.frames)};
}

}  // namespace s3::physics::assembly
