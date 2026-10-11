// Port of skate-core/src/air/body_spin.rs (whole file).
// Complete TU3 BodySpin::Update (82D8BF58) and history scan (82D8C2D8).
// Cached curves are explicit inputs, not reconstructed from Skate 2 settings.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::air::body_spin {

using point_graph::PointGraph;

/// Native bytes +0..176. Preserves the unused +136 word and mode-byte padding.
/// Construction/air-entry lifecycle belongs to the recovered caller, not Default.
class BodySpinState {
public:
    /// 82D8DA98..DAE0 initializes every BodySpin scalar and history entry.
    static BodySpinState new_() { return BodySpinState{}; }
    static BodySpinState from_words(std::array<std::uint32_t, 44> words) {
        if (!(words[42] < 30)) S3_PANIC("native derivative history index must be 0..30");
        BodySpinState s;
        s.words_ = words;
        return s;
    }
    const std::array<std::uint32_t, 44>& words() const { return words_; }
    /// +140, consumed by Reckoning::UpdateAirStates after BodySpin::Update.
    float speed() const { return get(140); }

    bool operator==(const BodySpinState& o) const { return words_ == o.words_; }
    bool operator!=(const BodySpinState& o) const { return !(*this == o); }

    // Private in Rust (module-visible); public here for the free functions below.
    float get(std::size_t offset) const { return rs::from_bits(words_[offset / 4]); }
    void set(std::size_t offset, float value) { words_[offset / 4] = rs::to_bits(value); }
    std::array<std::uint32_t, 44> words_{};
};

/// Native cached inputs at +176/+180, seven 80-byte curves, and 830BD300.
/// The vector threshold's producer must be supplied: no guessed retail default.
struct BodySpinSettings {
    float derivative_floor = 0.0f;
    float acceleration_limit = 0.0f;
    /// Native +184, +264, +344, +424, +504, +584, +664; x/y at +16/+48.
    std::array<PointGraph<8>, 7> curves{};
    std::array<float, 4> input_fade_threshold{};
};

/// Registers f1/f2/r6/r7, including the native mode-byte publication.
void update(BodySpinState& state, const BodySpinSettings& settings, float input, float auto_spin,
            bool in_air, std::uint8_t mode);

/// Ground caller82D8C8F0 supplies auto_spin=0, in_air=false and mode=0.
/// This branch does not read any of the airborne BodySpin tuning curves.
void update_ground(BodySpinState& state, float input);

}  // namespace s3::air::body_spin
