// Port of skate-core/src/air/trajectory/launch.rs (private module).
// Original TU3 trajectory launch calculations82D67D08/82D67320/82D682E8.
#pragma once
#include <cstdint>
#include <vector>

#include "s3/core/air/trajectory/math.hpp"
#include "s3/core/air/trajectory/prediction.hpp"
#include "s3/core/air/trajectory/types.hpp"

namespace s3::air::trajectory::launch {

using math::Vector;
using prediction::QueryRequest;
using prediction::Trajectory;
using types::LaunchInfo;
using types::SelectorInput;
using types::SelectorSettings;

/// pub(super)
bool adjust_velocity(LaunchInfo& info, SelectorInput input, const SelectorSettings& s);

/// pub(super)
std::vector<Vector> candidate_velocities(LaunchInfo info, const SelectorSettings& s);

/// pub(super)
struct LaunchBatch {
    std::vector<QueryRequest> requests;
    std::vector<Vector> velocities;
    Vector origin{};                // 2864
    Vector board_position{};        // 2848
    Vector local_board_position{};  // 2832
    Vector local_com_position{};    // 2784
    Vector com_displacement{};      // 2768
};

/// pub(super)
LaunchBatch batch(LaunchInfo info, SelectorInput input, const SelectorSettings& s);

/// 82D68698 deliberately uses the original -19.6 and -1/9.8 coefficients. (private)
float max_time(Vector velocity, const SelectorSettings& s);

/// tTrajectory::AdjustTrajectory82D609E0 changes velocity only. pub(super)
void adjust_trajectory(Trajectory& trajectory, std::int32_t frame, Vector adjustment, float maximum);

}  // namespace s3::air::trajectory::launch
