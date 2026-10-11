// Port of skate-core/src/input/gameplay_map.rs.
// Specialization of the 18 stock gameplay expressions registered by82697740.
#include "s3/core/input/gameplay_map.hpp"

namespace s3::input::gameplay_map {

GameplayActions GameplayActions::from_pad(const pad::Pad& pad) {
    auto value = [&pad](std::size_t slot) -> float {
        if (pad.count() == 0) {
            return 0.0f;
        }
        if (slot >= pad.records().size()) S3_PANIC("index out of bounds");
        return rs::from_bits(pad.records()[slot][0]);
    };
    GameplayActions a;
    a.values_ = {
        value(16) - value(17),
        value(18) - value(19),
        value(6),
        value(20) - value(21),
        value(22) - value(23),
        value(7),
        value(10),
        value(11),
        value(8),
        value(9),
        value(0),
        value(1),
        value(2),
        value(3),
        value(14),
        value(15),
        value(12),
        value(13),
    };
    return a;
}

float GameplayActions::value(std::uint32_t action) {
    if (action < 64) S3_PANIC("gameplay action below 64");
    const std::uint32_t index = action - 64;
    if (index >= values_.size()) S3_PANIC("index out of bounds");
    return values_[index];
}

std::uint8_t GameplayActions::state(std::uint32_t action) {
    return value(action) != 0.0f ? 1 : 0;
}

}  // namespace s3::input::gameplay_map
