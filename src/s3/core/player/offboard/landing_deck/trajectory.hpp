// Port of skate-core/src/player/offboard/landing_deck/trajectory.rs (free functions).
// Original82D78EE8 and its82D60B98/82D60C80,82D609E0,82D60B00 arithmetic.
// Manager::consider_board lives in landing_deck.cpp.
#pragma once
#include <cstdint>
#include <optional>

#include "s3/core/player/offboard/landing_deck/types.hpp"

namespace s3::player::offboard::landing_deck::trajectory {

using air::trajectory::Trajectory;

inline std::int32_t frames(float time) { return rs::as_i32(time * rs::from_bits(0x426fffffu)); }
float square_root(float value);
///82D60C80 ->82D60B98. No invented linear/zero-acceleration fallback.
std::optional<float> greatest_plane_time(Trajectory t, Vector point, Vector normal);
void adjust(Trajectory& t, std::int32_t frame, Vector delta, float maximum);
///82D60B00 leaves duration untouched.
void shift(Trajectory& t, std::int32_t frame);

}  // namespace s3::player::offboard::landing_deck::trajectory
