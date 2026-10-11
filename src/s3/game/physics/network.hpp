// Port of skate-game/src/physics/network.rs
//! Physics/transport boundary. Remote dynamic proxies join the existing solve;
//! only the locally owned assembly retains its solved reactions.
//! (Inactive multiplayer path; skate-net types come from the TEMP DEP shim `s3/game/_deps/skate_net.hpp`.)
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/board_step.hpp"
#include "s3/core/physics/board_world.hpp"
#include "s3/core/physics/rigid_body.hpp"
#include "s3/core/physics/world_contact.hpp"
#include "s3/game/_deps/skate_net.hpp"
#include "s3/game/animation_pose/glam.hpp"
#include "s3/game/common.hpp"
#include "s3/game/modding.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::network {

using glam::Mat4;
using glam::Vec3;
using ::s3::math::Vector3;
using ::s3::net::Body;
using ::s3::net::Bone;
using ::s3::net::Pose;
using ::s3::net::packed::BodyState;
using ::s3::net::packed::PoseState;
using ::s3::physics::assembly::BodySnapshot;
using ::s3::physics::board_world::BoardWorldVolume;
using ::s3::physics::world_contact::ContactPrimitive;

Pose pose(const Mat4& matrix);
Mat4 matrix(const Pose& p);
std::array<float, 3> xyz(Vector3 v);
Vector3 vector(std::array<float, 3> v);
Vector3 add(Vector3 a, Vector3 b);
Pose body_pose(const BodySnapshot& body);
std::vector<std::size_t> anchors(const SkaterRuntime& skater);
BodyState capture_body(const GamePhysics& physics, const SkaterRuntime& skater);
PoseState capture_pose(const SkaterRuntime& skater, const std::vector<std::size_t>& anchors);

/// Immutable stock geometry in each rigid body's COM frame; never transmitted.
struct Schema {
    std::vector<std::pair<std::size_t, BoardWorldVolume>> volumes;
    std::uint64_t fingerprint = 0;

    static rs::Result<Schema, std::string> new_(const GamePhysics& physics, const SkaterRuntime& skater);
};

ContactPrimitive transform(const ContactPrimitive& shape, const Mat4& m);
/// Conservative enclosing spheres for the remote broad phase.
std::pair<Vec3, float> bounds(const ContactPrimitive& p);

/// `#[derive(Default)]`.
struct Proxies {
    std::vector<BodySnapshot> bodies{};
    std::vector<BoardWorldVolume> volumes{};

    void append(const BodyState& frame, const Schema& schema, const GamePhysics& physics, const SkaterRuntime& skater,
                float age);
    /// Vehicle chassis join the existing native contact solver; reactions remain local.
    void append_vehicle(const modding::vehicles::network::CollisionShape& shape, const GamePhysics& physics,
                        const SkaterRuntime& skater);
};

}  // namespace s3::game::physics::network
