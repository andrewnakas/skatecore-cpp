// Port of skate-game/src/graph_host/motion_sliding_execute.rs (module `motion::slide_execution`).
//! Production lifecycle for the authored slide family; no detached state copies.
// The Rust file is an `impl MotionHost` block: MotionHost::execute_slide / slide_direction are declared in
// s3/game/graph_host/motion.hpp and defined in motion/slide_execution.cpp.
#pragma once
#include "s3/game/graph_host/motion.hpp"
