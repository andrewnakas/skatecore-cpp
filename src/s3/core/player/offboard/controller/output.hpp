// Port of skate-core/src/player/offboard/controller/output.rs.
// Original result exporter82D80F48 and frame helper82D2CDC0.
// `export` (needs controller::State) is declared in controller.hpp.
#pragma once
#include <array>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::controller {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;

struct GroundResult {
    Frame physical_frame{};      // result0 <- Biped64
    Frame animation_frame{};     // 64 <-128
    Frame surface_frame{};       // 128 <-82D2CDC0(axis416,forward32)
    Vector velocity{};           // 192 <-512
    Vector position{};           // 208 <-368
    float angular_velocity = 0.0f;  // 224 <-688
    bool alternate = false;         // 228 <-709
    bool sliding = false;           // 229 <-710
};
S3_DERIVE_EQ(GroundResult, physical_frame, animation_frame, surface_frame, velocity, position, angular_velocity,
             alternate, sliding)

/// pub(crate) frame helper82D2CDC0.
Frame build_frame(Vector up, Vector forward);

}  // namespace s3::player::offboard::controller
