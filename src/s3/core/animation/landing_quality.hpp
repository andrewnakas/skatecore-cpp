// Port of skate-core/src/animation/landing_quality.rs.
//! Original TU3 PhysicsConditioner::CalcLandingQuality, 0x82DE61D0.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::landing_quality {

using point_graph::PointGraph;

struct Settings {
    PointGraph<4> twist_spin;
    PointGraph<4> side_speed;
};

struct Input {
    /// PhysicsConditioner +144 and +148, after its filtered-state update.
    std::uint32_t previous_filtered_state = 0;
    std::uint32_t filtered_state = 0;
    /// PhysOut_Collision +32. Do not substitute a predicted landing normal.
    std::array<float, 4> ground_normal{};
    /// PhysOut_SkateboardMotion +80 and +273.
    std::array<float, 4> deck_velocity{};
    bool flipped = false;
    /// PhysOut_SkateboardReckoning +32: physical deck PART world Z from
    /// 82C02AD8/82C02B1C. This is not the conditioned Reckoning ground frame.
    std::array<float, 4> reckoning_forward{};
    /// PhysOut_Air +204.
    float air_spin = 0.0f;
};

/// Reset values are explicitly written by 0x82DE3F38.
struct Output {
    float landing_adjust_80 = 0.0f;
    float sideways_speed_84 = 0.0f;
    float forward_speed_88 = 0.0f;
    float spin_92 = 0.0f;
    std::uint32_t landing_type_96 = 0;
    bool landing_data_167 = false;

    /// Nonlanding frames preserve every field, including the valid-data latch.
    void update(Input input, const Settings& settings);
};
S3_DERIVE_EQ(Output, landing_adjust_80, sideways_speed_84, forward_speed_88, spin_92, landing_type_96,
             landing_data_167)

}  // namespace s3::animation::landing_quality
