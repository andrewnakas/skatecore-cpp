// Port of skate-core/src/input/graph_intents.rs.
// Native CreateMGIntentFromAGIntent and AttachIntent behavior boundaries.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "s3/core/rs.hpp"

namespace s3::input::graph_intents {

/// Complete82BA0C20. Ordinals come from82BA0A48 and stock filter names.
float apply_filter(float value, std::uint32_t kind);

/// Four source ordinals: filter1, filter2, fakieFilter, mirrorFilter.
/// Missing stance component bypasses every filter, matching82BA2930.
float filter_chain(float value, std::array<std::uint32_t, 4> filters,
                   std::optional<std::pair<bool, bool>> stance);

/// Rust `enum IntentMutation { None, Remove, Set(f32) }`.
struct IntentMutation {
    struct None {};
    struct Remove {};
    struct Set {
        float _0 = 0.0f;
    };
    std::variant<None, Remove, Set> value;
    IntentMutation() : value(None{}) {}
    IntentMutation(None v) : value(v) {}
    IntentMutation(Remove v) : value(v) {}
    IntentMutation(Set v) : value(v) {}
};
S3_DERIVE_EQ_EMPTY(IntentMutation::None)
S3_DERIVE_EQ_EMPTY(IntentMutation::Remove)
S3_DERIVE_EQ(IntentMutation::Set, _0)
S3_DERIVE_EQ(IntentMutation, value)

struct CreateMgIntent {
    bool on_update = false;
    std::optional<float> default_value;
    float scale = 0.0f;
    std::array<std::uint32_t, 4> filters{};

    /// Complete82BA2790 with container lookup/removal/insertion represented by
    /// an explicit mutation. Keys are the caller's decoded MGIntent/AGIntent.
    IntentMutation emit(std::optional<float> action_value, std::optional<std::pair<bool, bool>> stance) const;
    /// Complete82BA26F0 lifecycle write, including absent-input removal.
    IntentMutation enter(bool& created_this_frame, std::optional<float> action_value,
                         std::optional<std::pair<bool, bool>> stance) const;
    /// Complete82BA2728: onUpdate=false removes on subsequent updates.
    IntentMutation update(bool& created_this_frame, std::optional<float> action_value,
                          std::optional<std::pair<bool, bool>> stance) const;
    ///82BA2788 ->82BA28B8 unconditionally removes the owned MG intent.
    IntentMutation exit() const { return IntentMutation::Remove{}; }
};

/// Complete82BB5AE0 at resolved lookup/sink boundaries. The packet sink is
/// invoked first; optional skeleton sink receives the same value afterwards.
template <class P, class S>
inline void attach_intent(std::optional<float> value, bool set_skeleton, P&& packet, S&& skeleton) {
    if (value) {
        packet(*value);
        if (set_skeleton) {
            skeleton(*value);
        }
    }
}

}  // namespace s3::input::graph_intents
