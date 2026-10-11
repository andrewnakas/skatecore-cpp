// Port of skate-game/src/physics/input_teleport.rs
// (mounted as `physics::input_phase::teleport` via #[path] in input_phase.rs).
//! Ordinary local-player teleport82DB8998, operating on the live assemblies.
//! `impl Callbacks::reset_player` is a member of the private `input_phase::Callbacks`
//! struct (group D, physics/input_phase.hpp); its body lives in input_teleport.cpp.
#pragma once
#include "s3/core/animation/output/native.hpp"
#include "s3/core/physics/drive_frames.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::input_phase::teleport {

using sc::animation::output::NativeMatrix;

/// Private helpers.
sc::physics::drive_frames::RetailAffineTransform affine(NativeMatrix m);
///82DB8A10..8BB4 flattens At before normalization, rejects length<=.001,
///then adds the original .1m world-Y teleport clearance.
NativeMatrix horizontal_spawn(NativeMatrix requested);

}  // namespace s3::game::physics::input_phase::teleport
