// Port of skate-core/src/player/offboard/grab_scene/qualify/polyline.rs (whole file).
// Original82D2CFE8 nearest distance and82D2D2B0 arc-length evaluation.
#pragma once
#include "s3/core/player/offboard/grab_scene.hpp"

namespace s3::player::offboard::grab_scene::qualify_::polyline {

float nearest_distance(const Record& r, Vector world);
Vector at_distance(const Record& r, float distance);

}  // namespace s3::player::offboard::grab_scene::qualify_::polyline
