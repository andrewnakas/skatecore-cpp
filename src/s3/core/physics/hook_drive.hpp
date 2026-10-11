// Port of skate-core/src/physics/hook_drive/mod.rs.
// TU3 hook-drive state. The hook is a separate target part, child of deck 6.
// Registration, target publication and the caller's state transition remain
// explicit; changing these live dynamics does not recreate the drive.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/drive_parameters.hpp"

namespace s3::physics::hook_drive {

using drive_parameters::RetailDriveDynamics;
using drive_parameters::RetailDriveParams;
using drive_parameters::RetailDriveType;

struct HookDriveState {
    /// JointFrames at SkateboardBody+896: child quaternion/translation, parent
    /// quaternion/translation, four words each in guest lane order.
    std::array<std::uint32_t, 16> frames{};
    /// Live linear then angular parameters at SkateboardBody+960.
    std::array<std::uint32_t, 8> dynamics{};

    /// Frame/dynamics initialization in 82C0D330. This does not manufacture the
    /// separate hook assembly, allocate a drive, or choose a gameplay pose.
    static HookDriveState initial();

    /// 82C04708, including its inline linear enable and 82C0D5C0 call.
    /// TU3 reads no caller strength multiplier here.
    void enable_animation_soft(std::uint8_t& animated);

    /// Complete 82C0D5C0. Other fields and the caller's animated flag survive.
    void enable_angular_soft();

    /// Complete 82C05658: animated flag, angular soft, then linear hard/zero.
    void enable_angular_only(std::uint8_t& animated);

    /// Inline Ground Enter82D375D0..5FC and LetGo82D75450..98 operation.
    /// The blend flag and stored board quaternion are not inputs or outputs.
    void disable_animation(std::uint8_t& animated);

    void disable_linear();
    void disable_angular();

    RetailDriveDynamics solver_dynamics() const;
};
S3_DERIVE_EQ(HookDriveState, frames, dynamics)

}  // namespace s3::physics::hook_drive

#include "s3/core/physics/hook_drive/frames.hpp"
