// Port of skate-game/src/graph_host/motion_filter_execute.rs (module `motion::filter_execution`).
//! Live stock intent filtering shares the existing MotionAnimation maps.
// The Rust file is an `impl MotionHost` block: MotionHost::execute_intent_filter are declared in
// s3/game/graph_host/motion.hpp and defined in motion/filter_execution.cpp.
#pragma once
#include "s3/game/graph_host/motion.hpp"
