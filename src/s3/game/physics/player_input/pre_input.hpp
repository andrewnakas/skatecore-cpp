// Port of skate-game/src/physics/player_input/pre_input.rs.
//! Original82D81610 inactive-query branch and82D2DDD0 result reset. Pending
//! geometry is explicit; this owner never treats an unimplemented query as a miss.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::player_input::pre_input {

using sc::physics::skeleton_animation_record::AnimationPartTransform;

/// `#[derive(Clone, Debug)]`
struct PreInputResult {
    std::array<float, 4> position{};
    std::array<float, 4> normal{};
    AnimationPartTransform frame{};
    std::array<float, 4> vector96{};
    std::array<float, 4> vector112{};
    std::array<float, 4> vector128{};
    std::array<float, 4> vector144{};
    float scalar160 = 0.0f;
    std::uint32_t word164 = 0;
    float scalar168 = 0.0f;
    float scalar172 = 0.0f;
    std::uint32_t word176 = 0;
    std::uint32_t flags180 = 0;

    /// Private in Rust.
    static PreInputResult reset();
};

struct PreInputManager {
    bool pending_geometry = false;
    PreInputResult result{};
    std::array<std::uint32_t, 3> result_counts{};

    static PreInputManager new_();
    Status prepare(std::uint32_t& counter);
};

}  // namespace s3::game::physics::player_input::pre_input
