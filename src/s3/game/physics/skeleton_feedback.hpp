// Port of skate-game/src/physics/skeleton_feedback.rs.
//! Post-solver contact reports and physical/animation errors for the live skater.
//! Engine report storage is ours; spy arithmetic and report order are native.
#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "s3/core/physics/assembly.hpp"
#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/physics/skeleton_board_frames.hpp"
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/physics/skeleton_root.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::skeleton_feedback {

/// pub(super)
void publish(const GamePhysics& physics, SkaterRuntime& skater, bool request_partial_ragdoll);

/// Private in Rust (exercised by the ported test).
void publish_board_observations(sc::physics::skeleton_board_frames::SkeletonBoardFrames& frames,
                                const sc::physics::skeleton_root::SkeletonRootFrames& roots,
                                const sc::physics::skeleton_animation_record::AnimationPartTransform& deck,
                                std::array<float, 4> physical_com, float dt, std::uint32_t flags_2472);

/// Private in Rust.
std::vector<sc::physics::skeleton_body::SkeletonContactReport> collect(const GamePhysics& physics,
                                                                        const SkaterRuntime& skater);
/// Private in Rust. `Option<&'a BodySnapshot>` -> nullable pointer.
const sc::physics::assembly::BodySnapshot* resolve(std::uint32_t id, const GamePhysics& physics,
                                                   const SkaterRuntime& skater);
/// Private in Rust.
sc::physics::skeleton_body::SkeletonContactBody contact_body(const sc::physics::assembly::BodySnapshot* body);

}  // namespace s3::game::physics::skeleton_feedback
