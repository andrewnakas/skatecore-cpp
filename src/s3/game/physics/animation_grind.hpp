// Port of skate-game/src/physics/animation_grind.rs
// (`#[path = "animation_grind.rs"] mod animation_grind;` in physics/animation_phase.rs).
//! Completed physical publication -> both stock graphs. No state selection.
#pragma once
#include <array>
#include <string>
#include <tuple>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/physics/filtered_state.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/game/common.hpp"
#include "s3/game/graph_host/motion.hpp"
#include "s3/game/graph_host/motion_grind.hpp"
#include "s3/game/graph_host/motion_grind/conditions.hpp"

namespace s3::game::physics::animation_phase::animation_grind {

using ::s3::animation::output::attributes::AttributeName;
using ::s3::graph::conditions::PhysicalStateInputs;
using ::s3::physics::filtered_state::FilteredStateOutput;
using ::s3::player::input_phase::PhysicalPlayerInput;
namespace motion_grind = ::s3::game::graph_host::motion_grind;

/// `filtered: Option<&FilteredStateOutput>` -> nullable pointer.
rs::Result<PhysicalStateInputs, std::string> publish(::s3::game::graph_host::motion::MotionHost& host,
                                                     const PhysicalPlayerInput& physical,
                                                     const FilteredStateOutput* filtered, float height,
                                                     bool mirrored, std::array<float, 3> board_forward);

// Private in Rust (declared here for the ported tests).
rs::Result<std::tuple<PhysicalStateInputs, motion_grind::Physical, motion_grind::conditions::Physical>, std::string>
observations(const PhysicalPlayerInput& physical, const FilteredStateOutput* filtered, float height, bool mirrored,
             std::array<float, 3> board_forward);
std::array<float, 3> xyz(std::array<std::uint32_t, 4> raw);
/// Lossless adapter for CURRENT's string-based IsGrinding boundary. Stock grind
/// short names use only the native base38 alphabet 0-9,A-Z,_. This reverses
/// the existing encoder, then validates its round trip; it chooses no name.
rs::Result<std::string, std::string> name_text(AttributeName name);

}  // namespace s3::game::physics::animation_phase::animation_grind
