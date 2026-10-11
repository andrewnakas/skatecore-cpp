// Port of skate-core/src/input/controller.rs.
// TU3 RawControllerInput 82598FE0 and DerivedControllerInput 825992D8.
// Native action-map evaluation is an explicit boundary, not a platform mapping.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/rs.hpp"

namespace s3::input::controller {

/// Rust `trait ActionMap`.
/// Evaluated native cInputMap actions. IDs are TU3 map indices, not XInput
/// buttons. Implementations must preserve the call order; the stock expression
/// evaluator 8296C350 and loaded mapping still determine deadzones and scaling.
struct ActionMap {
    virtual ~ActionMap() = default;
    virtual float value(std::uint32_t action) = 0;
    virtual std::uint8_t state(std::uint32_t action) = 0;
};

class RawControllerInput {
public:
    static RawControllerInput from_words(std::array<std::uint32_t, 7> words) {
        RawControllerInput r;
        r.words_ = words;
        return r;
    }
    const std::array<std::uint32_t, 7>& words() const { return words_; }

    /// Complete 82598FE0, including simultaneous bumper arbitration and
    /// retention of the low twenty bits of the destination flags word.
    void update(const RawControllerInput& previous, ActionMap& map, bool state_502, bool state_104);

    bool operator==(const RawControllerInput& o) const { return words_ == o.words_; }
    bool operator!=(const RawControllerInput& o) const { return !(*this == o); }

    // Private in Rust (readable by DerivedControllerInput in the same module).
    /// Six binary32 axes/triggers followed by native packed button bits.
    std::array<std::uint32_t, 7> words_{};

private:
    void set_bit(std::uint32_t bit, std::uint8_t value);
};

/// Actual inputlistener attribute result for key 1F3C1C1C68AF4C23.
/// Missing collection/attribute uses the live 830D0850 fallback.
struct MagnitudeHeldSettings {
    std::optional<float> attribute;
    float missing_attribute_value = 0.0f;
};

class DerivedControllerInput {
public:
    /// Requires native initialized state. No reset or inferred default supplied.
    static DerivedControllerInput from_words(std::array<std::uint32_t, 26> words) {
        DerivedControllerInput d;
        d.words_ = words;
        return d;
    }
    const std::array<std::uint32_t, 26>& words() const { return words_; }

    /// Prefix construction recovered from TU3 `82599718`. The native
    /// constructor clears the controller axes and timers, preserves the low
    /// twenty flag bits in the two packed raw words, and seeds the four
    /// edge-timer fields from `flt_822F8B34` (`0x7effffff`). This is only the
    /// controller prefix; listener registration/reset belongs to its owner.
    void initialize();

    /// Complete 825992D8: raw history, trigger/button edge timers and stick
    /// magnitude-held timers. Timing uses caller dt without an imposed rate.
    void update(ActionMap& map, float timestep, bool state_502, bool state_104,
                const MagnitudeHeldSettings& settings);

    bool operator==(const DerivedControllerInput& o) const { return words_ == o.words_; }
    bool operator!=(const DerivedControllerInput& o) const { return !(*this == o); }

private:
    void advance_or_reset(std::size_t slot, bool advance, float timestep);
    std::array<std::uint32_t, 26> words_{};
};

/// pub(super) in Rust.
/// Independent reciprocal-square-root estimate followed by the original two
/// refinement steps. Host arithmetic is not claimed to be bit-exact Xenon math.
float magnitude(float squared);

}  // namespace s3::input::controller
