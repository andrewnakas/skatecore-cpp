// Port of skate-core/src/player/offboard/cadence.rs (whole file).
// Biped cadence producer82D80720 and phase helpers82D7AA18..82D7AFD0.
// Original TU3, SHA256431b8eba23565affdc10d137df19b06fe286244cefb3e1a13f32693e9600395a.
// Embed this state once in Biped; callers supply actual movement/animation input.
// PC floating-point arithmetic is not proven bit-exact Xenon arithmetic.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/rs.hpp"

namespace s3::player::offboard::cadence {

using Vector = std::array<float, 3>;

/// Retained Biped720..736; duration is unwritten by constructor82D7AFD8.
struct BipedPhase {
    float phase = 0.0f;
    float rate = 0.0f;
    float target = -1.0f;
    std::optional<float> duration;
    bool forward_target = false;

    ///82D7AD90: this fixed retail phase step is also used by Biped Air.
    void advance();
    // Rust-private helpers (exposed for the ported unit tests).
    void request(float target, float duration);
    void stop();
    ///82D7AB48, with82D7AA18's two candidate rates when neither wraps to1.
    void adjust_targets(float a, float b, float time, float upper);
    ///82D7AC30 selects the legal contact target closest to the current rate.
    void adjust_contact(float time, float lower, float upper);
};
S3_DERIVE_EQ(BipedPhase, phase, rate, target, duration, forward_target)

struct CadenceThresholds {
    std::array<float, 4> _0{};
    ///82D7AFD8: metrics come from actual walk/run/sprint AnimTransZ attributes.
    static CadenceThresholds from_clip_speeds(float walk, float run, float sprint) {
        return CadenceThresholds{{rs::from_bits(0x3c23d70au), (run + walk) * 0.5f, (sprint + run) * 0.5f,
                                  rs::from_bits(0x501502f9u)}};
    }
};
S3_DERIVE_EQ(CadenceThresholds, _0)

/// Offsets identify original Biped or job-input fields until their upstream
/// physical names are recovered. No field may be filled from a guessed proxy.
struct CadenceInput {
    Vector motion_512{};
    Vector motion_reference_272{};
    Vector reject_axis_400{};
    bool reject_enabled_708 = false;
    Vector up_144{};
    std::array<Vector, 3> frame_rows_0_16_32{};
    Vector frame_position_48{};
    Vector animation_motion_224{};
    float requested_duration_288 = 0.0f;
    float requested_phase_296 = 0.0f;
    bool suppress_adjustment_353 = false;
    std::uint32_t contact_flags_176 = 0;
    Vector contact_point_96{};
};

/// This is part of the physical Biped owner, not a MotionGraph phase clock.
struct BipedCadence {
    BipedPhase phase{};
    /// Original enum716. Values above every threshold retain the previous value.
    std::uint32_t locomotion_index = 0;

    ///82D80720. Called after movement stage82D80548, before common publication.
    void update(const CadenceInput& input, CadenceThresholds thresholds);
};
S3_DERIVE_EQ(BipedCadence, phase, locomotion_index)

// Rust-private free functions (exposed for the ported unit tests).
float select_rate(float a, float b, float lower, float upper, float both);
///82D7AF18: closed endpoints and signed floor count, not rem_euclid.
float wrap(float lower, float value, float upper);

}  // namespace s3::player::offboard::cadence
