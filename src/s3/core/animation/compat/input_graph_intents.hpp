// Compat for skate-core/src/input/graph_intents.rs (owned by the input stage).
// Uses the real port when it exists; otherwise a complete header-only port of the file.
// FALLBACK OWNED BY STAGE A: delete once s3/core/input/graph_intents.hpp exists.
#pragma once
#if __has_include("s3/core/input/graph_intents.hpp")
#include "s3/core/input/graph_intents.hpp"
#else
#include <array>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "s3/core/rs.hpp"

namespace s3::input::graph_intents {

/// Complete82BA0C20. Ordinals come from82BA0A48 and stock filter names.
inline float apply_filter(float value, std::uint32_t kind) {
    const float pi = rs::from_bits(0x40490fdb);
    const float half_pi = rs::from_bits(0x3fc90fdb);
    switch (kind) {
        case 0: return value;
        case 1: return value * -1.0f;
        case 2: return rs::abs(value);
        case 3: return value >= 0.0f ? 1.0f - value : -1.0f - value;
        case 4: {
            const float lower = -value >= 0.0f ? 0.0f : value;
            return 1.0f - lower >= 0.0f ? lower : 1.0f;
        }
        case 5: return value >= 0.0f ? pi - value : -pi - value;
        case 6: {
            const float rotated = value - half_pi;
            return rotated < -pi ? rotated + rs::from_bits(0x40c90fdb) : rotated;
        }
        default: {
            const float rotated = value + half_pi;
            return rotated > pi ? rotated - rs::from_bits(0x40c90fdb) : rotated;
        }
    }
}

/// Four source ordinals: filter1, filter2, fakieFilter, mirrorFilter.
/// Missing stance component bypasses every filter, matching82BA2930.
inline float filter_chain(float value, std::array<std::uint32_t, 4> filters,
                          std::optional<std::pair<bool, bool>> stance) {
    if (stance) {
        const std::array<bool, 4> enabled = {true, true, stance->first, stance->second};
        for (std::size_t i = 0; i < 4; ++i)
            if (enabled[i] && filters[i] <= 7) value = apply_filter(value, filters[i]);
    }
    return value;
}

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

    IntentMutation emit(std::optional<float> action_value,
                        std::optional<std::pair<bool, bool>> stance) const {
        const std::optional<float> v = action_value ? action_value : default_value;
        if (!v) return IntentMutation::Remove{};
        return IntentMutation::Set{filter_chain(scale * *v, filters, stance)};
    }
    IntentMutation enter(bool& created_this_frame, std::optional<float> action_value,
                         std::optional<std::pair<bool, bool>> stance) const {
        IntentMutation result = emit(action_value, stance);
        created_this_frame = true;
        return result;
    }
    IntentMutation update(bool& created_this_frame, std::optional<float> action_value,
                          std::optional<std::pair<bool, bool>> stance) const {
        IntentMutation result;
        if (on_update) {
            result = emit(action_value, stance);
        } else if (!created_this_frame) {
            result = IntentMutation::Remove{};
        } else {
            result = IntentMutation::None{};
        }
        created_this_frame = false;
        return result;
    }
    IntentMutation exit() const { return IntentMutation::Remove{}; }
};

template <class P, class S>
inline void attach_intent(std::optional<float> value, bool set_skeleton, P&& packet, S&& skeleton) {
    if (value) {
        packet(*value);
        if (set_skeleton) skeleton(*value);
    }
}

}  // namespace s3::input::graph_intents
#endif
