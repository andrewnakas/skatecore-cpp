// Port of skate-core/src/riding/pumping/geometry.rs.
// TU3 Pumping geometry82D8F280 and82D8F4A0..6AC. The reciprocal and
// reciprocal-square-root estimate primitives retain their documented host
// approximation boundary; source refinements, fused crosses and gates remain.
#pragma once
#include <array>

#include "s3/core/riding/pumping/controller.hpp"

namespace s3::riding::pumping::geometry {

using controller::PumpingSample;

/// Local stand-in for `core::convert::Infallible` (no value is ever constructed).
struct Infallible {};
S3_DERIVE_EQ_EMPTY(Infallible)

struct NativePumpingGeometry {
    using Error = Infallible;
    rs::Result<float, Error> height(std::array<float, 4> normal, std::array<float, 4> com);
    rs::Result<float, Error> speed(std::array<float, 4> previous, std::array<float, 4> current, float dt);
    rs::Result<float, Error> angular_speed(std::array<float, 4> previous_position,
                                           std::array<float, 4> previous_normal, const PumpingSample& sample,
                                           float dt);
    rs::Result<float, Error> inclination_radians(float y);
};

}  // namespace s3::riding::pumping::geometry
