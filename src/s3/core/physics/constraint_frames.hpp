// Port of skate-core/src/physics/constraint_frames.rs (pub(crate) items).
//
// Original TU3 constraint-frame arithmetic shared by joint and drive builders.
// 82AE0DB0 constructs quaternion-component rows directly from bilinear products.
// The builders compose frames and construct bases without normalizing inputs.
#pragma once
#include <array>

#include "s3/core/math.hpp"
#include "s3/core/physics/rigid_body.hpp"

namespace s3::physics::constraint_frames {

using math::Basis3;
using math::Vector3;
using rigid_body::RetailPackedWorldInverseInertia;
using rigid_body::RetailQuaternion;

struct QuaternionRows {
    std::array<Vector3, 3> axes;
    RetailQuaternion relative;
};

/// 82AE0DB0..0F88. Keep each rounded product and the original addition order;
/// rotating a relative quaternion's axes is only algebraically equivalent for
/// exact unit quaternions and has different finite-precision behavior.
QuaternionRows quaternion_rows(RetailQuaternion a, RetailQuaternion b);

/// Original quaternion multiplication at 82AE3C10..3C60 / 82AE1B94..1C4C.
RetailQuaternion compose(RetailQuaternion a, RetailQuaternion b);

/// Independently verified at Drive Build 82AE1CC4..1D90: sqrt(2) scaling,
/// 0.5 - component squared, paired residuals, and masks 822FB8A0/B0/C0.
/// This does not depend on Joint Build's unresolved RQD-gather shuffle at
/// 82AE3D98; it accepts an already known quaternion.
Basis3 basis(RetailQuaternion q);

Vector3 transform_direction(Basis3 b, Vector3 v);

Vector3 cross(Vector3 a, Vector3 b);

/// Source adds both cross-product terms directly into the incoming rate.
Vector3 point_rate(Vector3 linear, Vector3 angular, Vector3 arm);

Vector3 multiply_add(Vector3 v, float scalar, Vector3 addend);

float reciprocal(float value);

float reciprocal_sqrt(float value);

/// Drive 82AE2430..2490: X product, then the Y and Z fused additions.
Vector3 multiply_inertia(RetailPackedWorldInverseInertia t, Vector3 v);

/// Joint 82AE470C..4758 starts the same contraction with a fused +zero.
Vector3 multiply_inertia_from_zero(RetailPackedWorldInverseInertia t, Vector3 v);

}  // namespace s3::physics::constraint_frames
