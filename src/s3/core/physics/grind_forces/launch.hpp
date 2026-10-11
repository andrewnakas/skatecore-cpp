// Port of skate-core/src/physics/grind_forces/launch.rs.
// LaunchSkateboard82D73AB0; S2 named counterpart82DCB3A8.
// S3 replaces tunables with literals; energy cost is0.6, not ZIP's0.2.
#pragma once
#include "s3/core/physics/grind_contact/manager.hpp"
#include "s3/core/physics/grind_forces.hpp"

namespace s3::physics::grind_forces::launch {

using grind_contact::manager::Jumper;

struct Input {
    V velocity_400{};
    V position_112{};
    /// Low-energy zero-balance side selection uses CURRENT1120, not cache96.
    V current_point_1120{};
    float balance_2800 = 0.0f;
    float geometry_side_jump = 0.0f;
    float vertical_jump = 0.0f;
};

struct Output {
    V velocity{};
    /// Complete retained cache, to replace the SAME owner updated by82D739D8.
    Jumper jumper;
    std::uint32_t flags_2476_to_or = 0;
};

Output launch(Jumper jumper, Input input);

}  // namespace s3::physics::grind_forces::launch
