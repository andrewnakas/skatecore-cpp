// Port of skate-core/src/input/graph_intents.rs.
// Native CreateMGIntentFromAGIntent and AttachIntent behavior boundaries.
#include "s3/core/input/graph_intents.hpp"

namespace s3::input::graph_intents {

using rs::from_bits;

float apply_filter(float value, std::uint32_t kind) {
    const float pi = from_bits(0x40490fdbu);
    const float half_pi = from_bits(0x3fc90fdbu);
    switch (kind) {
        case 0:
            return value;
        case 1:
            return value * -1.0f;
        case 2:
            return rs::abs(value);
        case 3:
            return value >= 0.0f ? 1.0f - value : -1.0f - value;
        case 4: {
            const float lower = -value >= 0.0f ? 0.0f : value;
            return 1.0f - lower >= 0.0f ? lower : 1.0f;
        }
        case 5:
            return value >= 0.0f ? pi - value : -pi - value;
        case 6: {
            const float rotated = value - half_pi;
            return rotated < -pi ? rotated + from_bits(0x40c90fdbu) : rotated;
        }
        default: {
            const float rotated = value + half_pi;
            return rotated > pi ? rotated - from_bits(0x40c90fdbu) : rotated;
        }
    }
}

float filter_chain(float value, std::array<std::uint32_t, 4> filters,
                   std::optional<std::pair<bool, bool>> stance) {
    if (stance) {
        const std::array<bool, 4> enabled = {true, true, stance->first, stance->second};
        for (std::size_t i = 0; i < 4; ++i) {
            if (enabled[i] && filters[i] <= 7) {
                value = apply_filter(value, filters[i]);
            }
        }
    }
    return value;
}

IntentMutation CreateMgIntent::emit(std::optional<float> action_value,
                                    std::optional<std::pair<bool, bool>> stance) const {
    const std::optional<float> v = action_value ? action_value : default_value;
    if (!v) {
        return IntentMutation::Remove{};
    }
    return IntentMutation::Set{filter_chain(scale * *v, filters, stance)};
}

IntentMutation CreateMgIntent::enter(bool& created_this_frame, std::optional<float> action_value,
                                     std::optional<std::pair<bool, bool>> stance) const {
    IntentMutation result = emit(action_value, stance);
    created_this_frame = true;
    return result;
}

IntentMutation CreateMgIntent::update(bool& created_this_frame, std::optional<float> action_value,
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

}  // namespace s3::input::graph_intents
