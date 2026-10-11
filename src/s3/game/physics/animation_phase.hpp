// Port of skate-game/src/physics/animation_phase.rs.
//! The production physical-output -> stock graph -> next physical-input phase.
//! Native actor phases82592FD8/82593128/82593230/82593640 preserve this order.
//
// Submodules (#[path]): `grind` = physics/animation_grind.rs (group B, namespace
// animation_phase::grind), `packet` = physics/animation_phase_packet.rs.
#pragma once
#include <array>
#include <string>

#include "s3/core/animation/physical_feedback.hpp"
#include "s3/core/math.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/animation_phase_packet.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::animation_phase {

using packet::AnimationPhaseOutput;
using packet::AnimationProfile;

/// Observations are from the preceding completed physics tick. The current
/// controller map then advances both stock graphs; returned storage remains
/// alive while PlayerInput consumes packet(), hierarchy and ordered attributes.
rs::Result<AnimationPhaseOutput, std::string> advance(const GamePhysics& physics, SkaterRuntime& skater,
                                                      const PlayerControls& controls,
                                                      const graph_runtime::StockGraphs& graphs,
                                                      const AnimationProfile& profile);

/// Run once after completed physical output, before camera consumes that output.
/// Animation reads this same stored result on the following actor phase.
sc::animation::physical_feedback::PhysicalFeedback publish_feedback(const GamePhysics& physics,
                                                                     SkaterRuntime& skater);

/// PhysOutAnimation reset82DEFAA0 and the initial represented body outputs.
sc::animation::physical_feedback::PhysicalFeedback initial_feedback();

/// Private in Rust.
sc::math::Vector3 vec3(std::array<float, 4> v);
/// Private in Rust.
std::array<float, 4> vec4(sc::math::Vector3 v);
/// Private in Rust.
std::array<float, 4> axis(std::array<float, 3> v);

}  // namespace s3::game::physics::animation_phase
