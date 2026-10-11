// Port of skate-core/src/animation/ground_acceleration.rs.
//! Deck acceleration conditioning82DF0508 and IsBumped82BA7310, original TU3.
//! Matrices are the raw physical deck and Reckoning ground frames; the effective
//! animation deck is a different source. No render transform participates here.
#pragma once
#include <array>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::ground_acceleration {

using physics::skeleton_animation_record::AnimationPartTransform;

struct Input {
    /// PhysOutDeck0..48, actual Part6 transform (82C02AD8..2B20).
    AnimationPartTransform deck{};
    /// PhysOutGround0..48, Reckoning752..800 (82DB70E0..7100).
    AnimationPartTransform ground{};
    /// PhysOutMotion112, BoardBody528 from observed deck velocity differences.
    std::array<float, 4> world_acceleration{};
};

struct Settings {
    /// Globals344 anim_motion/bumps layout1700, scale_x_acc.
    float scale_x_acc = 0.0f;
    /// Same collection layout1768, min_bump_mag.
    float min_bump_mag = 0.0f;
};

struct Output {
    std::array<float, 4> acceleration{};
    bool bumped = false;
};

Output publish(Input input, const Settings& settings);

///82DF0508: remove deck-local Y, rotate back to world, express in the
///ground frame, then remove ground-local Y. Both original stores are required.
std::array<float, 4> condition(Input input);

///82BA7310 scales X only, then compares the refined length strictly > threshold.
///Independent Rust estimates are used; this is not a bit-exact Xenon claim.
bool is_bumped(std::array<float, 4> acceleration, const Settings& settings);

std::array<float, 4> inverse_rotate(const AnimationPartTransform& frame, std::array<float, 4> vector);
std::array<float, 4> rotate(const AnimationPartTransform& frame, std::array<float, 4> vector);

}  // namespace s3::animation::ground_acceleration
