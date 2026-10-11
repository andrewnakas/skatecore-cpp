// Port of skate-game/src/input/gesture_input.rs.
//! GameInputManager82696030/826962D8 -> listener8259B878/8259B9D0 ->
//! Fill8259B1F0..B7D8. Runs on the existing 60 Hz gameplay input boundary.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "s3/core/graph/intents.hpp"
#include "s3/core/input/gesture.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/vfs.hpp"

namespace s3::game::input::gesture_input {

struct GestureInput {
    // Private in Rust.
    std::vector<std::pair<std::size_t, ::s3::input::gesture::Recognizer>> recognizers;
    std::array<std::uint8_t, 2> maximum_misses{};
    std::optional<std::string> held_pattern;

    /// Rust `load(root: &Path)`.
    static rs::Result<GestureInput, std::string> load(const ::s3::data::Vfs& vfs, const std::string& asset_root);

    void publish(std::array<std::array<float, 2>, 2> axes, std::uint32_t difficulty, std::uint32_t flags,
                 std::uint32_t physical_state, ::s3::graph::intents::IntentMap& ag);
};

/// Private in Rust.
bool permitted(std::string_view name, std::uint32_t flags, std::uint32_t state,
               const ::s3::graph::intents::IntentMap& ag);

}  // namespace s3::game::input::gesture_input
