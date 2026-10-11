// Port of skate-core/src/physics/board_runtime.rs.
#include "s3/core/physics/board_runtime.hpp"

namespace s3::physics::board_runtime {

using board_pose::PoseMatrix;
using rigid_body::RetailBodyRates;
using rigid_body::RetailQuaternion;

std::uint32_t flags(BoardMotion mode) {
    switch (mode) {
        case BoardMotion::Active:
            return 4;
        case BoardMotion::Frozen:
            return 2;
        case BoardMotion::Static:
            return 1;
    }
    return 0;
}

namespace {

void copy_pose(const PartPose& part, RetailBodyRates& rates) {
    if (!part.body) S3_PANIC("transient pose adapter has a body");
    const auto& words = *part.body;
    auto f = [&](std::size_t i) { return rs::from_bits(words[i]); };
    rates.orientation = RetailQuaternion{f(0), f(1), f(2), f(3)};
    rates.position = Vector3{f(4), f(5), f(6)};
    rates.basis = Basis3{{{
        {f(16), f(17), f(18)},
        {f(20), f(21), f(22)},
        {f(24), f(25), f(26)},
    }}};
}

RetailAffineTransform rates_transform(const RetailBodyRates& rates) {
    return RetailAffineTransform{rates.basis, rates.position};
}

std::array<std::uint32_t, 44> body_pose_words(const RetailBodyRates& rates) {
    std::array<std::uint32_t, 44> words{};
    const RetailQuaternion q = rates.orientation;
    words[0] = rs::to_bits(q.x);
    words[1] = rs::to_bits(q.y);
    words[2] = rs::to_bits(q.z);
    words[3] = rs::to_bits(q.w);
    words[4] = rs::to_bits(rates.position.x);
    words[5] = rs::to_bits(rates.position.y);
    words[6] = rs::to_bits(rates.position.z);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t j = 0; j < 3; ++j) words[16 + axis * 4 + j] = rs::to_bits(rates.basis.columns[axis][j]);
    }
    return words;
}

std::array<std::uint32_t, 10> inertia_words(const RetailInertiaDynamics& inertia) {
    return rs::to_bits(std::array<float, 10>{
        inertia.inverse_tensor.x,
        inertia.inverse_tensor.y,
        inertia.inverse_tensor.z,
        0.0f,
        inertia.inverse_mass,
        inertia.spherical,
        inertia.maximum_linear_velocity,
        inertia.maximum_angular_velocity,
        inertia.linear_drag,
        inertia.angular_drag,
    });
}

PoseMatrix pose_words(const RetailAffineTransform& transform) {
    PoseMatrix words{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        for (std::size_t j = 0; j < 3; ++j) words[axis * 4 + j] = rs::to_bits(transform.basis.columns[axis][j]);
    }
    const Vector3 t = transform.translation;
    words[12] = rs::to_bits(t.x);
    words[13] = rs::to_bits(t.y);
    words[14] = rs::to_bits(t.z);
    return words;
}

PoseMatrix mass_frame_words(const RetailLocalMassFrame& frame) {
    return pose_words(RetailAffineTransform{frame.basis, frame.translation});
}

RetailAffineTransform transform_from_words(const PoseMatrix& words) {
    auto f = [&](std::size_t i) { return rs::from_bits(words[i]); };
    return RetailAffineTransform{
        Basis3{{{{f(0), f(1), f(2)}, {f(4), f(5), f(6)}, {f(8), f(9), f(10)}}}},
        Vector3{f(12), f(13), f(14)},
    };
}

}  // namespace

std::optional<std::string> BoardRuntime::take_solver_diagnostics(bool enabled) {
    step_.diagnostic_capture = enabled;
    std::optional<std::string> out = std::move(step_.diagnostic_snapshot);
    step_.diagnostic_snapshot.reset();
    return out;
}

BoardRuntime BoardRuntime::new_(std::array<RetailBodyMassProperties, BODY_COUNT> masses,
                                std::array<RetailAffineTransform, BODY_COUNT> authored, RetailAffineTransform spawn,
                                RetailSimulationStep simulation, BoardMotion mode) {
    std::array<PartPose, BODY_COUNT> parts{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        PartPose part;
        part.transform = pose_words(authored[i]);
        part.local_mass_frame = mass_frame_words(masses[i].local_mass_frame);
        part.body = std::array<std::uint32_t, 44>{};
        if (mode != BoardMotion::Static) part.inertia = inertia_words(masses[i].dynamics);
        board_pose::set_part_transform(part, pose_words(authored[i]));
        parts[i] = part;
    }
    PartPose hook_part;
    hook_part.transform = pose_words(RetailAffineTransform::IDENTITY);
    hook_part.local_mass_frame = std::nullopt;
    hook_part.body = std::array<std::uint32_t, 44>{};
    hook_part.inertia = std::nullopt;
    board_pose::set_board_transform(parts, hook_part, pose_words(spawn));
    BoardRuntime runtime;
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        runtime.bodies_[i] = initialized_body(parts[i], masses[i].dynamics, simulation, mode);
    }
    // CreateHookDrive 82C0D330 creates its own STATIC_BODY target. It does
    // not alias the deck or a global world body. Its mass calculation is
    // irrelevant to physical response because this body has no inertia.
    runtime.hook_ = BoardHook{
        initialized_body(hook_part, ZERO_INERTIA, simulation, BoardMotion::Static),
        hook_drive::HookDriveState::initial(),
    };
    for (std::size_t i = 0; i < BODY_COUNT; ++i) runtime.mass_frames_[i] = masses[i].local_mass_frame;
    runtime.step_ = BoardStep{};
    runtime.forces_ = BoardForceQueue{};
    runtime.collision_group_ = 4;
    return runtime;
}

void BoardRuntime::reset_physical(std::array<RetailAffineTransform, BODY_COUNT> authored,
                                  RetailAffineTransform target, std::uint32_t processed_flags, Vector3 gravity) {
    std::array<PartPose, BODY_COUNT> parts{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        parts[i].transform = pose_words(authored[i]);
        parts[i].local_mass_frame = mass_frame_words(mass_frames_[i]);
        parts[i].body = body_pose_words(bodies_[i].rates);
        parts[i].inertia = inertia_words(bodies_[i].inertia);
    }
    for (std::size_t i : {6u, 0u, 1u, 2u, 3u, 4u, 5u}) {
        board_pose::set_part_transform(parts[i], pose_words(authored[i]));
    }
    if ((processed_flags & 0x00100000u) != 0) {
        for (std::size_t i : {0u, 2u}) {
            for (float& v : target.basis.columns[i]) v = -v;
        }
    }
    PartPose hook;
    hook.transform = pose_words(hook_transform());
    hook.local_mass_frame = std::nullopt;
    hook.body = body_pose_words(hook_.body.rates);
    hook.inertia = std::nullopt;
    board_pose::set_board_transform(parts, hook, pose_words(target));
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        BodySnapshot& body = bodies_[i];
        copy_pose(parts[i], body.rates);
        body.rates.linear_velocity = Vector3::ZERO;
        body.rates.angular_velocity = Vector3::ZERO;
        body.rates.force_acceleration = gravity;
        body.rates.torque_acceleration = Vector3::ZERO;
        body.rates.world_inverse_inertia =
            rigid_body::world_inverse_inertia(body.rates.basis, body.inertia.inverse_tensor);
    }
    copy_pose(hook, hook_.body.rates);
    forces_.clear();
    // Completed contacts belong to the old pose; host storage can be
    // discarded without rebuilding physical bodies or their constraints.
    step_ = BoardStep{};
}

void BoardRuntime::set_transform(RetailAffineTransform target) {
    const auto authored = part_transforms();
    std::array<PartPose, BODY_COUNT> parts{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        parts[i].transform = pose_words(authored[i]);
        parts[i].local_mass_frame = mass_frame_words(mass_frames_[i]);
        parts[i].body = body_pose_words(bodies_[i].rates);
        parts[i].inertia = inertia_words(bodies_[i].inertia);
    }
    PartPose hook;
    hook.transform = pose_words(hook_transform());
    hook.local_mass_frame = std::nullopt;
    hook.body = body_pose_words(hook_.body.rates);
    hook.inertia = std::nullopt;
    board_pose::set_board_transform(parts, hook, pose_words(target));
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        BodySnapshot& body = bodies_[i];
        copy_pose(parts[i], body.rates);
        body.rates.world_inverse_inertia =
            rigid_body::world_inverse_inertia(body.rates.basis, body.inertia.inverse_tensor);
    }
    copy_pose(hook, hook_.body.rates);
}

void BoardRuntime::advance(Slice<const BoardCollision> collisions, std::array<float, 2> truck_targets,
                           BoardStepSettings settings) {
    step_.advance(bodies_, hook_, forces_, collisions, truck_targets, settings);
}

void BoardRuntime::advance_attached(Slice<const BoardCollision> collisions, std::array<float, 2> truck_targets,
                                    BoardStepSettings settings, AttachedStep attached) {
    step_.advance_attached(bodies_, hook_, forces_, collisions, truck_targets, settings, std::move(attached));
}

RetailAffineTransform BoardRuntime::body_transform(BodyId id) const {
    return rates_transform(bodies_[board::index(id)].rates);
}

std::array<RetailAffineTransform, BODY_COUNT> BoardRuntime::part_transforms() const {
    std::array<RetailAffineTransform, BODY_COUNT> out{};
    for (std::size_t i = 0; i < BODY_COUNT; ++i) {
        PartPose part;
        part.transform = pose_words(rates_transform(bodies_[i].rates));
        part.local_mass_frame = mass_frame_words(mass_frames_[i]);
        part.body = body_pose_words(bodies_[i].rates);
        part.inertia = std::nullopt;
        out[i] = transform_from_words(board_pose::part_transform(part));
    }
    return out;
}

RetailAffineTransform BoardRuntime::hook_transform() const { return rates_transform(hook_.body.rates); }

void BoardRuntime::set_hook_transform(RetailAffineTransform requested) {
    PartPose part;
    part.transform = pose_words(hook_transform());
    part.local_mass_frame = std::nullopt;
    part.body = body_pose_words(hook_.body.rates);
    part.inertia = std::nullopt;
    board_pose::set_part_transform(part, pose_words(requested));
    copy_pose(part, hook_.body.rates);
}

BodySnapshot initialized_body(const PartPose& part, RetailInertiaDynamics inertia, RetailSimulationStep simulation,
                              BoardMotion mode) {
    const bool is_static = mode == BoardMotion::Static;
    const RetailInertiaDynamics used = is_static ? ZERO_INERTIA : inertia;
    RetailBodyRates rates{};
    rates.orientation = RetailQuaternion::IDENTITY;
    rates.basis = RetailAffineTransform::IDENTITY.basis;
    rates.world_inverse_inertia = Basis3{};
    rates.position = Vector3::ZERO;
    rates.linear_velocity = Vector3::ZERO;
    rates.angular_velocity = Vector3::ZERO;
    rates.force_acceleration = is_static ? Vector3::ZERO : simulation.gravity_acceleration;
    rates.torque_acceleration = Vector3::ZERO;
    rates.kinetic_energy = is_static ? 0.0f : rs::f32::MAX;
    rates.cool_down = mode == BoardMotion::Active ? 0u : simulation.cool_down;
    copy_pose(part, rates);
    rates.world_inverse_inertia = rigid_body::world_inverse_inertia(rates.basis, used.inverse_tensor);
    return BodySnapshot{flags(mode), rates, used};
}

}  // namespace s3::physics::board_runtime
