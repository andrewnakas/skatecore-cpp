// Port of skate-core/src/physics/board_probes.rs.
// Deck and wall probes in TU3 82C07788/82C079E0, with the actual optional
// wall-line producer82C01F10. Wheel probes remain in board_ground.
#pragma once
#include <cstdint>
#include <optional>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_ground.hpp"

namespace s3::physics::board_probes {

using board_ground::WheelLine;
using math::Vector3;

struct BoardProbeHit {
    Vector3 point;
    Vector3 normal;
    std::uint32_t surface_tag = 0;
};
S3_DERIVE_EQ(BoardProbeHit, point, normal, surface_tag)

struct BoardProbeState {
    // SkateboardBody ctor82C06614..82C0675C clears both complete records.
    Vector3 start_;  // Rust field `start` (renamed: clashes with the `start` method).
    Vector3 point;
    Vector3 normal;
    std::uint32_t surface_tag = 0;
    bool hit = false;

    static BoardProbeState default_() { return BoardProbeState{}; }
    void start(Vector3 position) {
        start_ = position;
        hit = false;
    }
    /// A miss only clears the hit byte. Previously published point, normal and
    /// surface deliberately remain, unlike a disabled optional query.
    void publish(std::optional<BoardProbeHit> h) {
        hit = h.has_value();
        if (h) {
            point = h->point;
            normal = h->normal;
            surface_tag = h->surface_tag;
        }
    }
    void disable() { *this = default_(); }
};
S3_DERIVE_EQ(BoardProbeState, start_, point, normal, surface_tag, hit)

inline constexpr float DECK_PROBE_RADIUS = rs::from_bits(0x3e19999au);
/// Fifth query always runs from the physical deck origin100m down, radius0.15.
/// Native start is copied to body7968, and result to7984/8000/8016/8020.
inline WheelLine deck_probe(Vector3 position) {
    return WheelLine{position, Vector3{position.x, position.y + rs::from_bits(0xc2c80000u), position.z}};
}

struct WallLineInput {
    /// Processed2508: current physical state (100 or101 admitted).
    std::uint32_t state = 0;
    /// Processed464,544,112,2752.
    Vector3 contact_normal;
    Vector3 skater_up;
    Vector3 deck_position;
    float time = 0.0f;
};
/// Source82C01F10 clears endpoints208/224 on every call, then enables byte291
/// only for the three physical gates below. No guessed grounded condition.
std::optional<WheelLine> wall_probe(WallLineInput input);

}  // namespace s3::physics::board_probes
