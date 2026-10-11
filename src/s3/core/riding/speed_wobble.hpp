// Port of skate-core/src/riding/speed_wobble.rs.
// Complete TU3 SpeedWobble::CalculateSpeedWobble (82C0DBE8).
// UpdateSpeedWobble 82C03638 supplies the selected-mode attributes and body
// inputs; Ground Update 82D38800 applies the returned tilt before truck targets.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"

namespace s3::riding::speed_wobble {

using point_graph::PointGraph;

struct SpeedWobbleSettings {
    /// Layout arrays +0/+32, +64/+96, +128/+160, +192/+224.
    PointGraph<8> frequency_time{};
    PointGraph<8> frequency_speed{};
    PointGraph<8> amplitude_time{};
    PointGraph<8> amplitude_speed{};
    /// Layout +256 through +284, in native order.
    float tightness_threshold = 0.0f;
    float time_range = 0.0f;
    float speed_range = 0.0f;
    float frequency_scale = 0.0f;
    float amplitude_scale = 0.0f;
    float crouch_threshold = 0.0f;
    float height_min = 0.0f;
    float height_max = 0.0f;
};

/// Fields +16..+44, including untouched +20 and opaque/reset-only +24/+28.
/// The active flag is a byte; retain its other three packed bytes.
struct SpeedWobbleState {
    std::array<std::uint32_t, 8> _0{};

    /// The same six-float/one-byte reset used in Ground Enter 82D376D0.
    void reset() {
        for (std::size_t index : {0u, 2u, 3u, 4u, 5u, 6u}) {
            _0[index] = 0;
        }
        _0[7] &= 0x00ffffffu;
    }

    // Private in Rust.
    bool active() const { return (_0[7] >> 24) != 0; }
};
S3_DERIVE_EQ(SpeedWobbleState, _0)

struct SpeedWobbleInput {
    float tilt = 0.0f;
    /// Signed speed, ProcessedPhysIn +2612. This routine does not take abs().
    float speed = 0.0f;
    float center_of_mass_height = 0.0f;
    float truck_tightness = 0.0f;
    /// Resolved mode Hash_77AFCE78FE1206CA / Hash_5B57F2CCCCEEF430.
    float activation_threshold = 0.0f;
    float amplitude_multiplier = 0.0f;
};

float calculate(SpeedWobbleState& state, const SpeedWobbleSettings& settings, SpeedWobbleInput input);

}  // namespace s3::riding::speed_wobble
