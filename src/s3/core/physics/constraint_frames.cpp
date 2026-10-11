// Port of skate-core/src/physics/constraint_frames.rs.
#include "s3/core/physics/constraint_frames.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::physics::constraint_frames {

namespace {
Vector3 inertia_tail(RetailPackedWorldInverseInertia t, Vector3 v, Vector3 first) {
    return Vector3{
        rs::mul_add(t.full.z, v.z, rs::mul_add(t.full.y, v.y, first.x)),
        rs::mul_add(t.split.z, v.z, rs::mul_add(t.split.y, v.y, first.y)),
        rs::mul_add(t.split.x, v.z, rs::mul_add(t.split.z, v.y, first.z)),
    };
}
}  // namespace

QuaternionRows quaternion_rows(RetailQuaternion a, RetailQuaternion b) {
    const float xx = a.x * b.x;
    const float xy = a.x * b.y;
    const float xz = a.x * b.z;
    const float xw = a.x * b.w;
    const float yx = a.y * b.x;
    const float yy = a.y * b.y;
    const float yz = a.y * b.z;
    const float yw = a.y * b.w;
    const float zx = a.z * b.x;
    const float zy = a.z * b.y;
    const float zz = a.z * b.z;
    const float zw = a.z * b.w;
    const float wx = a.w * b.x;
    const float wy = a.w * b.y;
    const float wz = a.w * b.z;
    const float ww = a.w * b.w;
    return QuaternionRows{
        {
            Vector3{((ww + xx) - yy) - zz, ((yx + xy) + wz) + zw, ((xz + zx) - wy) - yw},
            Vector3{((yx + xy) - wz) - zw, ((ww + yy) - zz) - xx, ((yz + zy) + wx) + xw},
            Vector3{((xz + zx) + yw) + wy, ((zy + yz) - wx) - xw, ((ww + zz) - xx) - yy},
        },
        RetailQuaternion{
            ((wx - xw) + zy) - yz,
            ((wy - yw) + xz) - zx,
            ((wz - zw) + yx) - xy,
            ((ww + zz) + yy) + xx,
        },
    };
}

RetailQuaternion compose(RetailQuaternion a, RetailQuaternion b) {
    const Vector3 c = cross(Vector3{a.x, a.y, a.z}, Vector3{b.x, b.y, b.z});
    return RetailQuaternion{
        rs::mul_add(a.x, b.w, rs::mul_add(b.x, a.w, c.x)),
        rs::mul_add(a.y, b.w, rs::mul_add(b.y, a.w, c.y)),
        rs::mul_add(a.z, b.w, rs::mul_add(b.z, a.w, c.z)),
        a.w * b.w - native_arithmetic::dot3({a.x, a.y, a.z, 0.0f}, {b.x, b.y, b.z, 0.0f}),
    };
}

Basis3 basis(RetailQuaternion q) {
    const float root_two = rs::from_bits(0x3fb504f3u);
    const float x = q.x * root_two;
    const float y = q.y * root_two;
    const float z = q.z * root_two;
    const float w = q.w * root_two;
    const float dx = rs::mul_add(-x, x, 0.5f);
    const float dy = rs::mul_add(-y, y, 0.5f);
    const float dz = rs::mul_add(-z, z, 0.5f);
    const float xy = x * y;
    const float yz = y * z;
    const float zx = z * x;
    const float wz = w * z;
    const float wx = w * x;
    const float wy = w * y;
    return Basis3{{{
        {dy + dz, xy + wz, zx - wy},
        {xy - wz, dz + dx, yz + wx},
        {zx + wy, yz - wx, dx + dy},
    }}};
}

Vector3 transform_direction(Basis3 b, Vector3 v) {
    const auto& c = b.columns;
    return Vector3{
        rs::mul_add(c[2][0], v.z, rs::mul_add(c[1][0], v.y, c[0][0] * v.x)),
        rs::mul_add(c[2][1], v.z, rs::mul_add(c[1][1], v.y, c[0][1] * v.x)),
        rs::mul_add(c[2][2], v.z, rs::mul_add(c[1][2], v.y, c[0][2] * v.x)),
    };
}

Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3{
        rs::mul_add(-a.z, b.y, a.y * b.z),
        rs::mul_add(-a.x, b.z, a.z * b.x),
        rs::mul_add(-a.y, b.x, a.x * b.y),
    };
}

Vector3 point_rate(Vector3 linear, Vector3 angular, Vector3 arm) {
    return Vector3{
        rs::mul_add(arm.z, angular.y, rs::mul_add(-arm.y, angular.z, linear.x)),
        rs::mul_add(arm.x, angular.z, rs::mul_add(-arm.z, angular.x, linear.y)),
        rs::mul_add(arm.y, angular.x, rs::mul_add(-arm.x, angular.y, linear.z)),
    };
}

Vector3 multiply_add(Vector3 v, float scalar, Vector3 addend) {
    return Vector3{
        rs::mul_add(v.x, scalar, addend.x),
        rs::mul_add(v.y, scalar, addend.y),
        rs::mul_add(v.z, scalar, addend.z),
    };
}

float reciprocal(float value) {
    float r = native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float residual = rs::mul_add(-r, value, 1.0f);
        r = rs::mul_add(r, residual, r);
    }
    return r;
}

float reciprocal_sqrt(float value) {
    float r = native_arithmetic::reciprocal_square_root_estimate(value);
    for (int i = 0; i < 2; ++i) {
        const float squared = r * r;
        const float half = r * 0.5f;
        const float residual = rs::mul_add(-value, squared, 1.0f);
        r = rs::mul_add(half, residual, r);
    }
    return r;
}

Vector3 multiply_inertia(RetailPackedWorldInverseInertia t, Vector3 v) {
    return inertia_tail(t, v, Vector3{t.full.x * v.x, t.full.y * v.x, t.full.z * v.x});
}

Vector3 multiply_inertia_from_zero(RetailPackedWorldInverseInertia t, Vector3 v) {
    return inertia_tail(t, v,
                        Vector3{rs::mul_add(t.full.x, v.x, 0.0f), rs::mul_add(t.full.y, v.x, 0.0f),
                                rs::mul_add(t.full.z, v.x, 0.0f)});
}

}  // namespace s3::physics::constraint_frames
