// Port of skate-core/src/trigonometry.rs: scalar lane of TU3 `XMVectorSinCos`, `0x82473A08`.
#pragma once
#include <utility>

namespace s3::trigonometry {

/// Scalar lane of TU3 `XMVectorSinCos`, `0x82473A08`. Returns (sin, cos).
std::pair<float, float> sin_cos(float angle);
/// Standalone TU3 cosine82473930.
float cos(float angle);
/// TU3 vector sine 824531C8.
float sin(float angle);
/// Scalar lane of TU3 vector acos 82453298.
float acos(float value);
/// Shared inverse-sine polynomial, also inlined by DisablePushBrake82BA5150.
float asin(float value);

}  // namespace s3::trigonometry
