// Port of skate-core/src/physics/triangle_query.rs.
#include "s3/core/physics/triangle_query.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/triangle_sweep.hpp"

namespace s3::physics::triangle_query {

bool triangle_segment(TriangleLineHit& result, Vector3 start, Vector3 direction,
                      std::array<Vector3, 3> vertices, float line_radius, float triangle_fatness) {
    const float radius = line_radius + triangle_fatness;
    if (radius == 0.0f) {
        if (auto hit = thin_triangle(start, direction, vertices)) {
            result = *hit;
            return true;
        }
        return false;
    }
    const Vector3 normal = cross(sub(vertices[0], vertices[1]), sub(vertices[0], vertices[2]));
    result.normal = scale(normal, inverse_length(dot(normal, normal)));
    if (triangle_sweep::sweep(result, start, direction, vertices, radius)) {
        result.position = sub(result.position, scale(result.normal, line_radius));
        return true;
    }
    return false;
}

std::optional<TriangleLineHit> thin_triangle(Vector3 start, Vector3 direction,
                                             std::array<Vector3, 3> vertices) {
    const Vector3 a = vertices[0], b = vertices[1], c = vertices[2];
    const Vector3 ac = sub(c, a);
    const Vector3 ab = sub(b, a);
    Vector3 perpendicular = cross(direction, ac);
    const float determinant = dot(ab, perpendicular);
    if (!(determinant > rs::from_bits(0x322bcc77u))) {
        return std::nullopt;
    }
    const Vector3 from_a = sub(start, a);
    const float lower = determinant * rs::from_bits(0xb727c5acu);
    const float upper = determinant - lower;
    const float u = dot(from_a, perpendicular);
    if (u < lower || u > upper) {
        return std::nullopt;
    }
    perpendicular = cross(from_a, ab);
    const float v = dot(direction, perpendicular);
    if (v < lower || v + u > upper) {
        return std::nullopt;
    }
    const float distance = dot(ac, perpendicular);
    if (distance < lower || distance > upper) {
        return std::nullopt;
    }
    const float inverse = 1.0f / determinant;
    const float fraction = distance * inverse;
    // Dispatcher82ADF61C..688 recomputes and normalizes this winding with
    // two refinements; it does not use the Volume's cached contact normal.
    const Vector3 normal = cross(sub(a, b), sub(a, c));
    return TriangleLineHit{
        madd(direction, fraction, start),
        scale(normal, inverse_length(dot(normal, normal))),
        fraction,
        {u * inverse, v * inverse, 0.0f},
    };
}

float dot(Vector3 a, Vector3 b) {
    return native_arithmetic::dot3({a.x, a.y, a.z, 0.0f}, {b.x, b.y, b.z, 0.0f});
}
Vector3 sub(Vector3 a, Vector3 b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 scale(Vector3 v, float scale) { return Vector3{v.x * scale, v.y * scale, v.z * scale}; }
Vector3 madd(Vector3 a, float scale, Vector3 b) {
    return Vector3{rs::mul_add(a.x, scale, b.x), rs::mul_add(a.y, scale, b.y),
                   rs::mul_add(a.z, scale, b.z)};
}
Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3{
        rs::mul_add(-a.z, b.y, a.y * b.z),
        rs::mul_add(-a.x, b.z, a.z * b.x),
        rs::mul_add(-a.y, b.x, a.x * b.y),
    };
}
float inverse_length(float squared) {
    float inverse = native_arithmetic::reciprocal_square_root_estimate(squared);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-squared, inverse * inverse, 1.0f), inverse);
    }
    return inverse;
}

}  // namespace s3::physics::triangle_query
