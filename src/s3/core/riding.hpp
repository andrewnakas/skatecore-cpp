// Port of skate-core/src/riding/mod.rs.
// Recovered riding calculations. Inputs are processed physics inputs, not
// platform stick/button values; action-graph producers remain a separate owner.
#pragma once
#include "s3/core/riding/anti_flip.hpp"
#include "s3/core/riding/braking.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/ground_contact_response.hpp"
#include "s3/core/riding/ground_correction_math.hpp"
#include "s3/core/riding/ground_force.hpp"
#include "s3/core/riding/ground_orientation.hpp"
#include "s3/core/riding/grounded.hpp"
#include "s3/core/riding/heading.hpp"
#include "s3/core/riding/pumping.hpp"
#include "s3/core/riding/push.hpp"
#include "s3/core/riding/push_animation.hpp"
#include "s3/core/riding/push_behaviors.hpp"
#include "s3/core/riding/reckoning_frames.hpp"
#include "s3/core/riding/slide_friction.hpp"
#include "s3/core/riding/speed_and_slope.hpp"
#include "s3/core/riding/speed_model.hpp"
#include "s3/core/riding/speed_wobble.hpp"
#include "s3/core/riding/steering.hpp"
#include "s3/core/riding/straighten.hpp"
// `mod vector;` is private: s3/core/riding/vector.hpp.
