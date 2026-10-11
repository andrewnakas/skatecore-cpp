// Port of skate-core/src/player/offboard/ground_reckoning.rs (whole file).
// Original Reckoning::UpdateBiped82D8E3E0, using the existing shared owners.
// 82D8E458 explicitly clears velocity; riding acceleration/damping is absent.
// This adapter follows the shared XYZ orientation representation; all input
// frame/filter math uses four lanes and returns complete computed vectors.
#pragma once
#include <array>

#include "s3/core/air/body_spin.hpp"
#include "s3/core/air/reckoning/data.hpp"
#include "s3/core/math.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/riding/ground_orientation.hpp"
#include "s3/core/riding/reckoning_frames.hpp"

namespace s3::player::offboard::ground_reckoning {

using Vector = std::array<float, 4>;

struct Settings {
    std::array<float, 4> ground_normal_smoothing{};
    const point_graph::PointGraph<8>* tilt_vs_rotation = nullptr;
    const point_graph::PointGraph<8>* tilt_vs_slope = nullptr;
};
struct Input {
    /// Actual current reckoning1152, supplied as all four original lanes.
    Vector previous_up{};
    Vector requested_up{};
    Vector requested_forward{};
    float blend = 0.0f;
    bool reverse_stance = false;
    bool enable_body_spin_input = false;
    float physical_body_spin_2812 = 0.0f;
};
/// Four-lane publications let the canonical owner retain every computed lane.
/// No separate persistent orientation state is introduced here.
struct Output {
    Vector dynamic_up_1136{};
    Vector up_1152{};
    Vector target_1168{};
    Vector velocity_1184{};
    Vector ground_normal_1216{};
};

Output update(riding::ground_orientation::GroundOrientation& orientation,
              riding::reckoning_frames::ReckoningFrames& frames, air::body_spin::BodySpinState& body_spin,
              air::reckoning::data::AirState& air, const Settings& settings, const Input& input);

inline s3::math::Vector3 xyz(Vector v) { return s3::math::Vector3::new_(v[0], v[1], v[2]); }

}  // namespace s3::player::offboard::ground_reckoning
