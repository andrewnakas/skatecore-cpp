// Port of skate-core/src/riding/grounded/manual_entry.rs.
// Ground's manual entry slice82D376F4..7750 and velocity helper82D37960.
// Original-image bytes confirm the complete entry-helper branch/write order.
// The game adapter supplies live board bodies and the shared dot calculation.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/manual/state.hpp"

namespace s3::riding::grounded::manual_entry {

using physics::manual::state::ManualEntryContinuation;
using physics::manual::state::ManualState;

struct ManualGroundInput {
    /// ProcessedPhysIn+2720; either signed zero bypasses every body operation.
    float balance = 0.0f;
    /// ProcessedPhysIn+464, used directly without normalization.
    std::array<float, 4> ground_normal{};
    /// ProcessedPhysIn+2468. Bit20 swaps the two contact-controlled groups.
    std::uint32_t flags_2468 = 0;
    /// ProcessedPhysIn+2472. Bits26/27 select the two groups.
    std::uint32_t flags_2472 = 0;
};

/// Rust trait `ManualGroundBodies` (duck-typed template parameter `B`):
/// Direct part-body velocity access, not a force queue or delayed write list.
/// Resolve the live board assembly part on each call: native reads follow every
/// earlier write, including when multiple parts happen to refer to one body.
///   /// Part indices0..6 resolve body pointers at assembly+76+96*part; velocity
///   /// is the four-component body field+32. No angular velocity is touched.
///   std::array<float, 4> linear_velocity(std::size_t part);
///   void set_linear_velocity(std::size_t part, std::array<float, 4> velocity);
///
/// Rust trait `ManualGroundProjection` (template parameter `P`):
/// Three-component projection supplied by the host's shared numerical adapter.
///   using Error = ...;
///   /// Native three-component projection at82D3799C (and six repeated sites).
///   /// W does not contribute. Independent hardware numerical parity of the
///   /// shared dot implementation is separate from this helper's control flow.
///   rs::Result<float, Error> normal_speed(std::array<float, 4> ground_normal,
///                                         std::array<float, 4> linear_velocity);

namespace detail {
template <class B, class P>
rs::Result<rs::Unit, typename P::Error> remove_part(std::size_t part, std::array<float, 4> ground_normal, B& bodies,
                                                    P& projection) {
    const std::array<float, 4> velocity = bodies.linear_velocity(part);
    const auto normal_speed = projection.normal_speed(ground_normal, velocity);
    if (normal_speed.is_err()) return rs::make_err(normal_speed.unwrap_err());
    std::array<float, 4> remaining = velocity;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        // Separate multiply then subtract82D379A4..A8, not a fused operation.
        const float component = ground_normal[axis] * normal_speed.unwrap();
        remaining[axis] = velocity[axis] - component;
    }
    // W is retained bit-for-bit by the native insert before each store.
    bodies.set_linear_velocity(part, remaining);
    return rs::Unit{};
}
}  // namespace detail

/// Full branch/write order of82D37960, conditional on the required projection.
/// Despite the recovered name, both signs of normal motion are removed. There
/// is no clamp, normal-length division, timestep or balance-sign selection.
template <class B, class P>
rs::Result<rs::Unit, typename P::Error> remove_velocity_into_ground(ManualGroundInput input, B& bodies,
                                                                    P& projection) {
    if (input.balance == 0.0f) {
        return rs::Unit{};
    }
    {
        auto r = detail::remove_part(6, input.ground_normal, bodies, projection);
        if (r.is_err()) return r;
    }

    // Native flags are read after the deck write82D379B4..B8.
    const bool reversed = (input.flags_2468 & (1u << 20)) != 0;
    const bool contact26 = (input.flags_2472 & (1u << 26)) != 0;
    const bool contact27 = (input.flags_2472 & (1u << 27)) != 0;
    const bool first_group = reversed ? contact26 : contact27;
    const bool second_group = reversed ? contact27 : contact26;
    if (first_group) {
        for (std::size_t part : {std::size_t(0), std::size_t(1), std::size_t(4)}) {
            auto r = detail::remove_part(part, input.ground_normal, bodies, projection);
            if (r.is_err()) return r;
        }
    }
    if (second_group) {
        for (std::size_t part : {std::size_t(2), std::size_t(3), std::size_t(5)}) {
            auto r = detail::remove_part(part, input.ground_normal, bodies, projection);
            if (r.is_err()) return r;
        }
    }
    return rs::Unit{};
}

/// Performs the actual manual-state reset/scaling, then its required velocity
/// continuation before returning to Ground Enter's subsequent category200 work.
/// An error retains the executed state/body prefix and must abort the tick.
template <class B, class P>
rs::Result<rs::Unit, typename P::Error> enter_ground(ManualState& state, std::uint32_t previous_category,
                                                     float powerslide_exit_scale, ManualGroundInput input,
                                                     B& bodies, P& projection) {
    if (state.enter_ground(previous_category, powerslide_exit_scale) ==
        ManualEntryContinuation::RemoveVelocityIntoGround) {
        auto r = remove_velocity_into_ground(input, bodies, projection);
        if (r.is_err()) return r;
    }
    return rs::Unit{};
}

}  // namespace s3::riding::grounded::manual_entry
