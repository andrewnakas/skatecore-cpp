// Port of skate-core/src/player/offboard/surface_frame.rs (whole file).
// Original Biped82D7E2A8 surface normal and82D7E840 spring/lean stages.
// Only these stages' written fields are retained here; all vectors keep VMX w.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::surface_frame {

using Vector = std::array<float, 4>;
inline constexpr Vector UP{0.0f, 1.0f, 0.0f, 0.0f};  // private const in Rust

struct SurfaceInput {
    std::uint32_t flags = 0;  // input176
    Vector normal{};          // input16
    Vector origin{};          // input0
    Vector edge_point{};      // input128
    Vector edge_normal{};     // input144
};
struct SpringInput {
    Vector velocity{};        // Biped480
    Vector up{};              // Biped16
    Vector right{};           // Biped0
    Vector forward{};         // Biped32
    Vector spring_right{};    // Biped128
    Vector spring_forward{};  // Biped160
    float right_delta = 0.0f;    // Biped696
    float forward_delta = 0.0f;  // Biped692
    bool suppress_lean = false;  // input317
};

struct State {
    Vector surface_normal = UP;  // 544
    Vector source_normal = UP;   // 560
    Vector spring_normal = UP;   // 416
    Vector spring_delta{};       // 432
    Vector final_up = UP;        // 448
    Vector lean{};               // 464

    void update_surface(const SurfaceInput& input);
    void update_spring(const SpringInput& input);
};
S3_DERIVE_EQ(State, surface_normal, source_normal, spring_normal, spring_delta, final_up, lean)

}  // namespace s3::player::offboard::surface_frame
