// Port of skate-core/src/physics/hook_drive/mod.rs.
#include "s3/core/physics/hook_drive.hpp"

namespace s3::physics::hook_drive {

namespace {
std::array<std::uint32_t, 4> soft(float spring, float strength) {
    const float damping = (spring * frames::rsqrt(spring)) * 2.0f;
    return {rs::to_bits(spring), rs::to_bits(damping), rs::to_bits(strength), 1u};
}
}  // namespace

HookDriveState HookDriveState::initial() {
    HookDriveState state{};
    const std::array<std::uint32_t, 12> identity = rs::to_bits(std::array<float, 12>{
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f});
    set_child_angular_frame(state.frames, identity);
    set_parent_angular_frame(state.frames, identity);
    state.disable_linear();
    state.disable_angular();
    return state;
}

void HookDriveState::enable_animation_soft(std::uint8_t& animated) {
    animated = 1;
    const auto s = soft(5000.0f, rs::from_bits(0x44e0fffeu));
    for (std::size_t i = 0; i < 4; ++i) dynamics[i] = s[i];
    enable_angular_soft();
}

void HookDriveState::enable_angular_soft() {
    const auto s = soft(20000.0f, rs::from_bits(0x470c9fffu));
    for (std::size_t i = 0; i < 4; ++i) dynamics[4 + i] = s[i];
}

void HookDriveState::enable_angular_only(std::uint8_t& animated) {
    animated = 1;
    enable_angular_soft();
    disable_linear();
}

void HookDriveState::disable_animation(std::uint8_t& animated) {
    animated = 0;
    disable_linear();
    disable_angular();
}

void HookDriveState::disable_linear() {
    dynamics[0] = 0;
    dynamics[1] = 0;
    dynamics[2] = 0;
    dynamics[3] = 2;
}

void HookDriveState::disable_angular() {
    dynamics[4] = 0;
    dynamics[5] = 0;
    dynamics[6] = 0;
    dynamics[7] = 2;
}

RetailDriveDynamics HookDriveState::solver_dynamics() const {
    auto params = [&](std::size_t i) {
        RetailDriveType type;
        switch (dynamics[i + 3]) {
            case 1: type = RetailDriveType::SoftDrive; break;
            case 2: type = RetailDriveType::HardDrive; break;
            default: type = RetailDriveType::NoDrive; break;
        }
        return RetailDriveParams{rs::from_bits(dynamics[i]), rs::from_bits(dynamics[i + 1]),
                                 rs::from_bits(dynamics[i + 2]), type};
    };
    return RetailDriveDynamics{params(0), params(4)};
}

}  // namespace s3::physics::hook_drive
