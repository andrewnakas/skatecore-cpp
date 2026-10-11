// Port of skate-core/src/player/offboard/ground_input.rs (whole file).
// Original TU3 ground control input82D310F8; S3 SHA256
// 431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
// Pure calculation: the existing BipedGround STATE owns the inputs and outputs.
// Stock curves must be present and validated at the data boundary.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::ground_input {

struct GroundInput {
    std::uint32_t processed_flags_2472 = 0;
    float processed_direct_2684 = 0.0f;
    float processed_direct_2680 = 0.0f;
    float processed_stick_2692 = 0.0f;
    float processed_stick_2688 = 0.0f;
    float processed_scale_2912 = 0.0f;
    float processed_scale_2908 = 0.0f;
    /// Forward row of the actual Biped frame, state112.
    std::array<float, 3> frame_forward_112{};
};

/// Ground STATE offsets:708/712 become job-input308/312 (job base400).
/// These are not Biped controller708, which is a separate correction boolean.
struct GroundInputOutput {
    float state_708 = 0.0f;
    float state_712 = 0.0f;
    /// Original untransformed input, NOT its coordinates in the flattened frame.
    std::array<float, 4> state_656{};
};
S3_DERIVE_EQ(GroundInputOutput, state_708, state_712, state_656)

/// 82D310F8. Curve A key DF759B46440F16E9; curve B key2DD95B399BAE313E
/// (TurnVsStickAngle). Leading PointNegGraphData8 bounds are not inputs here.
GroundInputOutput calculate(const GroundInput& input, const point_graph::PointGraph<8>& curve_a,
                            const point_graph::PointGraph<8>& turn_vs_stick_angle);

}  // namespace s3::player::offboard::ground_input
