// Port of skate-game/src/physics/grind_chromosome/pose.rs (module physics::grind_chromosome::pose).
#pragma once
#include <array>
#include <optional>

#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/core/rs.hpp"
#include "s3/game/physics/grind/family.hpp"

namespace s3::game::physics::grind_chromosome::pose {

using grind::Family;
using V = std::array<float, 4>;
using ::s3::riding::ground_correction_math::dot_product;
/// `pub(super) use skate_core::riding::ground_correction_math::dot_product as dot;`
inline float dot(V a, V b) { return dot_product(a, b); }

/// Consumed subset of the144-byte record copied by82DEEAF8. Ground80 and
/// Skeleton144/160 are copied by the original but not read by these producers.
struct ApproachPose {
    V right{};
    V position{};
    std::array<V, 2> feet{};
    bool fakie = false;
};

struct Input {
    std::uint32_t category = 0;
    bool grinding_316 = false;
    bool air_event_439 = false;
    ///None represents the native reset/unknown family, not FiftyFifty.
    std::optional<Family> family;
    V basic_right_0{};
    V basic_forward_32{};
    V basic_position_48{};
    V basic_location_axis_96{};
    V basic_twist_axis_128{};
    V basic_location_position_144{};
    std::array<V, 2> feet_256_272{};
    bool fakie_155 = false;
    V point{};
    V direction{};
    V normal{};
    V across{};
};

///82DEED98: twist uses Basic128, not Basic32 or a substituted deck-forward.
bool straight(const Input& input);
///82DEEEE0: sign the Basic32 axis from Basic48 minus contact.
bool low(const Input& input);
inline V add(V a, V b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline V sub(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline V signed_(V v, bool positive) {
    return {positive ? v[0] : -v[0], positive ? v[1] : -v[1], positive ? v[2] : -v[2], positive ? v[3] : -v[3]};
}

}  // namespace s3::game::physics::grind_chromosome::pose
