// Port of skate-game/src/physics/climbing/ledge.rs
//! Static collision probes for the first authored jump/hang/mantle set.
#pragma once
#include <array>
#include <optional>
#include <utility>

#include "s3/core/math.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/game/animation_pose/glam.hpp"
#include "s3/game/common.hpp"
#include "s3/game/transform_shim.hpp"

namespace s3::game::physics::climbing::ledge {

using glam::Vec3;
using ::s3::physics::board_world::BoardWorld;

struct Ledge {
    Vec3 anchor{};
    Vec3 landing{};
    Vec3 forward{};
    std::array<Vec3, 2> palms{};
    std::array<Vec3, 2> normals{};
};

::s3::math::Vector3 v(Vec3 p);
std::optional<std::pair<Vec3, Vec3>> hit(const BoardWorld& world, Vec3 a, Vec3 b, float radius);
std::optional<Ledge> find(const BoardWorld& world, Vec3 feet, Vec3 facing);
std::optional<Ledge> find_air(const BoardWorld& world, Vec3 feet, Vec3 facing);
std::optional<Ledge> find_range(const BoardWorld& world, Vec3 feet, Vec3 facing, float minimum, float distance);
bool clear(const BoardWorld& world, Ledge ledge);

}  // namespace s3::game::physics::climbing::ledge
