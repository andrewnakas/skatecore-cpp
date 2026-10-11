// Port of skate-core/src/physics/grind_forces/release.rs.
// Exit nudge82D3F850 / S2 82D85330. Family dispatch remains separate.
#pragma once
#include "s3/core/physics/grind_forces.hpp"

namespace s3::physics::grind_forces::release {

struct Input {
    V position{};
    V point{};
    V across{};
    V normal{};
    V velocity{};
    std::uint32_t geometry_kind = 0;
    V high_side{};
    bool force_across = false;
    /// Original r7. It is independent of r8/force_across; slides suppress lift.
    bool enable_lift = false;
    float strength = 0.0f;
    float speed_limit = 0.0f;
    float lift = 0.0f;
};

/// None means no AddForce call, including no accumulator wakeup.
inline std::optional<V> force(Input input) {
    const V outward =
        input.force_across || input.geometry_kind == 0
            ? scale(input.across, dot3(input.across, sub(input.position, input.point)) > 0.0f ? 1.0f : -1.0f)
            : scale(input.high_side, -1.0f);
    if (!(dot3(input.velocity, outward) < input.speed_limit)) {
        return std::nullopt;
    }
    const V lateral = scale(outward, input.strength);
    if (!input.enable_lift) {
        return lateral;
    }
    const float upward = rs::max(input.normal[1], 0.0f) * input.lift;
    V out{};
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(input.normal[i], upward, lateral[i]);
    return out;
}

}  // namespace s3::physics::grind_forces::release
