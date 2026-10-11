// Port of skate-game/src/physics/handplant/contact.rs
//! PositionSelector82D63D40, velocity window82D64420 and edge clipping82D651E0.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/physics/slice.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/handplant/settings.hpp"

namespace s3::game::physics::handplant::contact {

using V = std::array<float, 4>;  // super::V
using settings::Settings;
using ::s3::physics::grind_contact::Primitive;

/// #[derive(Clone, Copy)]
struct Candidate {
    V point{};
    Primitive edge{};
    std::int32_t side = 0;
};

std::optional<Candidate> select(const Settings& settings, V com, V velocity, V normal, std::int32_t hint,
                                ::s3::physics::Slice<const Primitive> edges);

//82D658A0 excludes coplanar segments and avoids dividing near-parallel Z.
bool clip_depth(std::array<V, 2>& segment, float value, bool above);
bool clip(std::array<V, 2>& segment, float a, float b);

}  // namespace s3::game::physics::handplant::contact
