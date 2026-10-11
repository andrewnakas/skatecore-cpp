// Port of skate-core/src/input/gameplay_map.rs.
// Specialization of the 18 stock gameplay expressions registered by82697740.
// Config identity and VM operand semantics are recorded in STEERING_INPUT.md.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/input/controller.hpp"
#include "s3/core/input/pad.hpp"
#include "s3/core/rs.hpp"

namespace s3::input::gameplay_map {

/// Implements Rust's `impl ActionMap for GameplayActions`.
struct GameplayActions : controller::ActionMap {
    // Private in Rust.
    std::array<float, 18> values_{};

    GameplayActions() = default;
    GameplayActions(const GameplayActions&) = default;
    GameplayActions& operator=(const GameplayActions&) = default;

    static GameplayActions from_values(std::array<float, 18> values) {
        GameplayActions a;
        a.values_ = values;
        return a;
    }
    const std::array<float, 18>& values() const { return values_; }

    /// The native pad retains storage even while count is zero. With a nonzero
    /// count, stock Button operands read their fixed index, without a per-index
    /// count check; the caller must provide the native 24-button storage.
    static GameplayActions from_pad(const pad::Pad& pad);

    /// Panics ("gameplay action below 64") for action < 64.
    float value(std::uint32_t action) override;
    std::uint8_t state(std::uint32_t action) override;
};
inline bool operator==(const GameplayActions& a, const GameplayActions& b) { return a.values_ == b.values_; }
inline bool operator!=(const GameplayActions& a, const GameplayActions& b) { return !(a == b); }

}  // namespace s3::input::gameplay_map
