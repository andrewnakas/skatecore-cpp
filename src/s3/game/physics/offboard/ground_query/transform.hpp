// Port of skate-game/src/physics/offboard/ground_query/transform.rs
#pragma once
#include <array>
#include <cstdint>

#include "s3/game/physics/offboard/ground_query.hpp"

namespace s3::game::physics::offboard::ground_query::transform {

/// pub(super)
Vector3 add(Vector3 a, Vector3 b);
/// pub(super)
Vector3 sub(Vector3 a, Vector3 b);
/// pub(super)
Vector3 scale(Vector3 a, float s);
/// private
Vector3 madd(Vector3 a, float s, Vector3 b);
/// pub(super)
Vector3 point(const Frame& f, Vector3 p);
/// pub(super)
Bounds bounds(const Frame& f, Bounds b);
/// pub(super)
bool overlaps(Bounds a, Bounds b);
/// pub(super)
bool matches(std::int32_t a, std::int32_t b);
//82ACA4E0..550: face is formed from decoded WORLD vertices and refined twice.
/// pub(super)
Vector3 face(const std::array<Vector3, 3>& v);

}  // namespace s3::game::physics::offboard::ground_query::transform
