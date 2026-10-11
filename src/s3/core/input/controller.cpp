// Port of skate-core/src/input/controller.rs.
// TU3 RawControllerInput 82598FE0 and DerivedControllerInput 825992D8.
#include "s3/core/input/controller.hpp"

#include <algorithm>

#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::input::controller {

using rs::from_bits;
using rs::mul_add;
using rs::to_bits;

void RawControllerInput::update(const RawControllerInput& previous, ActionMap& map, bool state_502,
                                bool state_104) {
    const std::uint32_t value_actions[6] = {64, 65, 67, 68, 70, 71};
    for (std::size_t slot = 0; slot < 6; ++slot) {
        words_[slot] = to_bits(map.value(value_actions[slot]));
    }
    const std::uint32_t state_actions[4][2] = {{66, 31}, {69, 30}, {72, 29}, {73, 28}};
    for (const auto& pair : state_actions) {
        const std::uint8_t state = map.state(pair[0]);
        set_bit(pair[1], state);
    }
    const std::uint32_t both = (1u << 29) | (1u << 28);
    if ((words_[6] & both) == both) {
        const std::uint32_t prior = previous.words_[6] & both;
        if ((prior == 1u << 29 || prior == 1u << 28) && (state_502 || state_104)) {
            words_[6] = (words_[6] & ~both) | prior;
        } else {
            words_[6] &= ~(1u << 28);
        }
    }
    for (std::uint32_t action = 74; action <= 81; ++action) {
        const std::uint8_t state = map.state(action);
        set_bit(101 - action, state);
    }
}

void RawControllerInput::set_bit(std::uint32_t bit, std::uint8_t value) {
    words_[6] = (words_[6] & ~(1u << bit)) | (std::uint32_t(value & 1u) << bit);
}

void DerivedControllerInput::initialize() {
    const std::uint32_t flags_last = words_[6] & 0x000fffffu;
    const std::uint32_t flags_current = words_[13] & 0x000fffffu;
    words_ = {};
    words_[6] = flags_last;
    words_[13] = flags_current;
    std::fill(words_.begin() + 16, words_.begin() + 20, 0x7effffffu);
}

void DerivedControllerInput::update(ActionMap& map, float timestep, bool state_502, bool state_104,
                                    const MagnitudeHeldSettings& settings) {
    std::array<std::uint32_t, 7> prev_words{};
    std::copy(words_.begin() + 7, words_.begin() + 14, prev_words.begin());
    const RawControllerInput previous = RawControllerInput::from_words(prev_words);
    std::copy(previous.words().begin(), previous.words().end(), words_.begin());
    RawControllerInput current = previous;
    current.update(previous, map, state_502, state_104);
    std::copy(current.words().begin(), current.words().end(), words_.begin() + 7);
    const std::size_t axis_slots[2][2] = {{14, 4}, {15, 5}};
    for (const auto& pair : axis_slots) {
        advance_or_reset(pair[0], from_bits(current.words_[pair[1]]) == 1.0f, timestep);
    }
    const std::uint32_t before = previous.words_[6];
    const std::uint32_t now = current.words_[6];
    const std::uint32_t edges[2][3] = {{31, 16, 17}, {30, 18, 19}};
    for (const auto& e : edges) {
        const bool was_down = (before & (1u << e[0])) != 0;
        const bool is_down = (now & (1u << e[0])) != 0;
        advance_or_reset(e[1], !(!was_down && is_down), timestep);
        advance_or_reset(e[2], !(was_down && !is_down), timestep);
    }
    const std::uint32_t timers[4][2] = {{20, 80}, {21, 81}, {22, 78}, {23, 79}};
    for (const auto& pair : timers) {
        const bool held = map.state(pair[1]) != 0;
        advance_or_reset(pair[0], held, timestep);
    }
    // Native evaluates these actions again after the button timers.
    const float lx = map.value(64);
    const float ly = map.value(65);
    const float rx = map.value(67);
    const float ry = map.value(68);
    const float right = magnitude(mul_add(ry, ry, rx * rx));
    const float left = magnitude(mul_add(ly, ly, lx * lx));
    const float threshold = settings.attribute ? *settings.attribute : settings.missing_attribute_value;
    // PPC ble treats unordered as false; retain that branch direction.
    advance_or_reset(24, !(left <= threshold), timestep);
    advance_or_reset(25, !(right <= threshold), timestep);
}

void DerivedControllerInput::advance_or_reset(std::size_t slot, bool advance, float timestep) {
    words_[slot] = advance ? to_bits(from_bits(words_[slot]) + timestep) : 0u;
}

float magnitude(float squared) {
    float inverse = physics::reciprocal_sqrt::estimate(squared);
    for (int i = 0; i < 2; ++i) {
        const float correction = mul_add(-squared, inverse * inverse, 1.0f);
        inverse = mul_add(inverse * 0.5f, correction, inverse);
    }
    if (squared == 0.0f) {
        return 0.0f;
    }
    return squared * inverse;
}

}  // namespace s3::input::controller
