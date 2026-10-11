// Port of skate-core/src/player/offboard/slide.rs (whole file).
// Biped special-mode latch82D7C8E8 and sliding velocity82D7CC18.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::offboard::slide {

using Vector = std::array<float, 4>;

struct SpecialMode {
    bool enabled_714 = false;
    float elapsed_788 = 0.0f;

    void update(float move_input_308, std::uint32_t contact_flags_176, float forward_y) {
        if (enabled_714) {
            elapsed_788 += rs::from_bits(0x3c888889u);
            if (!(move_input_308 >= 0.5f) || (!(elapsed_788 <= 0.5f) && (contact_flags_176 & 0x200u) == 0)) {
                enabled_714 = false;
            }
        } else if (!(elapsed_788 <= 0.0f)) {
            if (rs::from_bits(0x3e19999au) > forward_y) {
                elapsed_788 = 0.0f;
            }
        } else if ((contact_flags_176 & 0x200u) != 0) {
            elapsed_788 = 0.0f;
            enabled_714 = true;
        }
    }
};
S3_DERIVE_EQ(SpecialMode, enabled_714, elapsed_788)

struct Input {
    bool special_mode_714 = false;
    Vector surface_normal_560{};
    Vector movement_velocity_480{};
    std::optional<Vector> contact_direction_400;
};

struct Sliding {
    Vector velocity_528{};
    bool active_710 = false;

    void update(const Input& input, const point_graph::PointGraph<8>& slide_vs_slope,
                const point_graph::PointGraph<8>& slide_vs_speed);
};
S3_DERIVE_EQ(Sliding, velocity_528, active_710)

}  // namespace s3::player::offboard::slide
