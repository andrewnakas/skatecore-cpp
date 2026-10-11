// Port of skate-core/src/player/wipeout_state/response.rs (whole file).
// Original Wipeout response82D3E678 and retained-velocity82D3E348/82D3E8C0.
#pragma once
#include <array>
#include <optional>

#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/player/wipeout_state/data.hpp"
#include "s3/core/player/wipeout_state/drives.hpp"
#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::wipeout_state::response {

using math::V;

///82D3C2B0: only upward collision/prediction normals are accepted.
V response_normal(std::optional<V> support, std::optional<V> prediction);
void trigger(State& state, physics::skeleton_body::SkeletonBody& body, V normal, std::array<float, 2> input);
void update_counter(State& state, bool contact);
void retained_velocity(State& state, physics::skeleton_body::SkeletonBody& body,
                       physics::skeleton_body::SkeletonDrives& targets, const drives::Settings& settings, bool contact,
                       std::uint32_t flags2468, V com_velocity);

}  // namespace s3::player::wipeout_state::response
