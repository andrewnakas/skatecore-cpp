// Port of simref sim/src/camera/shake_data.rs.
//! Owned .shk parser; numerical sequence centering follows82E05DC0 in core.
#pragma once
#include <string>

#include "s3/core/camera/shake.hpp"

namespace s3::game::camera::shake_data {

rs::Result<s3::camera::ShakeSamples, std::string> parse(const std::string& text);

}  // namespace s3::game::camera::shake_data
