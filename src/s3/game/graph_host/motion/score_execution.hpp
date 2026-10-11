// Port of skate-game/src/graph_host/motion_score_execute.rs (module `motion::score_execution`).
//! Stateless score and bump lifecycles using the same MotionHost publication.
// The Rust file is an `impl MotionHost` block: MotionHost::execute_score_or_bump are declared in
// s3/game/graph_host/motion.hpp and defined in motion/score_execution.cpp.
#pragma once
#include "s3/game/graph_host/motion.hpp"
