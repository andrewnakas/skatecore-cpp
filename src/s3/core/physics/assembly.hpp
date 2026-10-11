// Port of skate-core/src/physics/assembly.rs.
// Board constraint construction from physical body snapshots.
//
// The default TU3 assembly has six joints, two truck drives and a hook drive. Wheel drives
// are an alternate configuration, disabled in the recovered stock assembly.
// World collision and gameplay force production do not belong in this module.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/core/physics/drive_parameters.hpp"
#include "s3/core/physics/drive_solver.hpp"
#include "s3/core/physics/hook_drive.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/solver.hpp"

namespace s3::physics::assembly {

using board::BODY_COUNT;
using board::BodyId;
using drive_frames::RetailAffineTransform;
using drive_frames::RetailDriveFrames;
using drive_parameters::RetailDriveDynamics;
using drive_solver::RetailDriveRows;
using rigid_body::RetailBodyRates;
using rigid_body::RetailInertiaDynamics;
using solver::JointConstraint;

struct BodySnapshot {
    std::uint32_t state_flags = 0;
    RetailBodyRates rates;
    RetailInertiaDynamics inertia;
};

/// Separate animated target body registered by CreateHookDrive 82C0D330.
/// It is not one of the seven physical board parts and must not be substituted
/// for the deck. Startup supplies its native body state explicitly.
struct BoardHook {
    BodySnapshot body;
    hook_drive::HookDriveState drive;
};

inline constexpr std::size_t HOOK_REACTION = BODY_COUNT;

/// The caller owns the snapshots, force accumulators and tick timing. No pose
/// is adjusted here to force the board onto a plane or into an animation.
struct BoardConstraints {
    /// Eligible rows only; retained in native registration order.
    std::vector<JointConstraint> joints;
    /// Front truck, back truck, then hook; native creation/insertion order.
    std::vector<RetailDriveRows> drives;

    static BoardConstraints build(const std::array<BodySnapshot, BODY_COUNT>& bodies, const BoardHook& hook,
                                  std::array<RetailDriveFrames, 3> frames, RetailDriveDynamics truck_dynamics,
                                  float delta_seconds);
};

/// SetTruckDriveFrames 82C0B9C0 followed by the active-drive quaternion
/// preparation in Island::Step_Solver2 82763B08. Hook normalization is a live
/// state write; it is not deferred to the animation frame setters.
std::array<RetailDriveFrames, 3> prepare_drive_frames(std::array<RetailAffineTransform, 2> base,
                                                      std::array<float, 2> targets, BoardHook& hook);

}  // namespace s3::physics::assembly
