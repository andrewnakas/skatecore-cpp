// Port of skate-game/src/graph_host/motion_execute_leaf.rs (module `motion::leaf_execution`).
#pragma once
#include <cstdint>
#include <string>

#include "s3/core/rs.hpp"
#include "s3/game/graph_host/motion.hpp"

namespace s3::game::graph_host::motion::leaf_execution {

/// `pub(super)` in Rust.
rs::Result<rs::Unit, std::string> execute(MotionHost& host, BehaviorId behavior, MotionOperation operation,
                                          const Frame& frame, std::uint8_t phase);

}  // namespace s3::game::graph_host::motion::leaf_execution
