// Port of skate-core/src/physics/triangle_sweep.rs.
#include "s3/core/physics/triangle_sweep.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/triangle_closest.hpp"
#include "s3/core/physics/triangle_sweep_walk.hpp"

namespace s3::physics::triangle_sweep {

using triangle_closest::closest;
using triangle_query::cross;
using triangle_query::dot;
using triangle_query::madd;
using triangle_query::scale;
using triangle_query::sub;

namespace {
float l1(Vector3 v) { return (rs::abs(v.y) + rs::abs(v.z)) + rs::abs(v.x); }
}  // namespace

bool sweep(TriangleLineHit& result, Vector3 start, Vector3 delta, std::array<Vector3, 3> vertices,
           float radius) {
    result.fraction = 0.0f;
    const Vector3 a = vertices[0], b = vertices[1], c = vertices[2];
    const Vector3 ab = sub(b, a);
    const Vector3 ac = sub(c, a);
    Vector3 from_a = sub(start, a);
    const Vector3 cross_ac = cross(delta, ac);
    float determinant = dot(ab, cross_ac);
    float side = 1.0f;
    if (determinant < 0.0f) {
        side = -1.0f;
        determinant = -determinant;
    }
    if (dot(result.normal, from_a) * side < -radius) {
        return false;
    }
    const float u = dot(from_a, cross_ac) * side;
    // This TU3 body uses the L1 sum, not the SDK's vector magnitude bound.
    const float u_margin = l1(cross_ac) * radius;
    if (u < -u_margin || u > u_margin + determinant) {
        return false;
    }
    const Vector3 cross_ab = cross(ab, delta);
    const float v = dot(from_a, cross_ab) * side;
    const float v_margin = l1(cross_ab) * radius;
    if (v < -v_margin || v > v_margin + determinant || v + u > (u_margin + v_margin) + determinant) {
        return false;
    }
    if (determinant > MIN_RECIPROCAL) {
        const Vector3 offset = sub(from_a, scale(result.normal, side * radius));
        const float distance = -(dot(offset, cross(ac, ab)) * side);
        if (distance > determinant) {
            return false;
        }
        if (!(distance < 0.0f)) {
            const float inverse = 1.0f / determinant;
            result.fraction = inverse * distance;
            const float u2 = dot(offset, cross_ac) * side;
            if (!(u2 < 0.0f) && !(u2 > determinant)) {
                const float v2 = dot(offset, cross_ab) * side;
                if (!(v2 < 0.0f) && !(v2 + u2 > determinant)) {
                    result.position = madd(delta, result.fraction, start);
                    result.normal = scale(result.normal, side);
                    result.volume_parameter = {inverse * u2, inverse * v2, 0.0f};
                    return true;
                }
            }
            from_a = madd(delta, result.fraction, from_a);
            delta = scale(delta, 1.0f - result.fraction);
        }
    }
    const Vector3 start2 = add(from_a, a);
    const auto feature = closest(start2, vertices);
    const Vector3 separation = sub(start2, feature.point);
    const float squared = dot(separation, separation);
    const float penetration = rs::mul_add(radius, radius, -squared);
    if (penetration > 0.0f) {
        result.position = start2;
        if (squared > 0.0f) {
            float inverse = native_arithmetic::reciprocal_square_root_estimate(squared);
            inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-squared, inverse * inverse, 1.0f), inverse);
            result.normal = scale(separation, inverse);
        }
        result.volume_parameter = {feature.u, feature.v, penetration};
        return true;
    }
    if (feature.region == 6) {
        result.position = start2;
        //82ADEFA0 compares the absolute determinant retained in f1; it has
        // already been negated by82ADEC30 for a reverse-facing query.
        if (determinant < 0.0f) {
            result.normal = scale(result.normal, -1.0f);
        }
        result.volume_parameter = {feature.u, feature.v, 0.0f};
        return true;
    }
    return triangle_sweep_walk::walk(result, start2, delta, vertices, feature, radius);
}

Vector3 add(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }

}  // namespace s3::physics::triangle_sweep
