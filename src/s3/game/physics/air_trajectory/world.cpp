// Port of skate-game/src/physics/air_trajectory/world.rs.
//! Original trajectory collision leaf82771D08 and nearby triangle collector
//!82772028, across the host's world-format boundary. BoardWorld stores static
//!world-space triangles, so their source geometry transform is identity.
#include "s3/game/physics/air_trajectory/world.hpp"

#include <limits>

#include "s3/core/physics/triangle_query.hpp"

namespace s3::game::physics::air_trajectory::world {

using ::s3::math::Vector3;
using ::s3::physics::triangle_query::triangle_segment;
using ::s3::physics::triangle_query::TriangleLineHit;

rs::Result<std::optional<SurfaceHit>, std::string> line(const BoardWorld& world, Vector start, Vector end,
                                                        float radius) {
    bool non_finite = false;
    for (float v : start) non_finite = non_finite || !std::isfinite(v);
    for (float v : end) non_finite = non_finite || !std::isfinite(v);
    if (!std::isfinite(radius) || radius < 0.0f || non_finite) {
        return rs::make_err(std::string("Non-finite trajectory collision request or invalid radius"));
    }
    const Vector3 s = vec3(start);
    const Vector3 delta = Vector3::new_(end[0] - s.x, end[1] - s.y, end[2] - s.z);
    float nearest = rs::f32::MAX;
    std::optional<SurfaceHit> result;
    for (const auto& [index, entry] : world.line_candidates(s, vec3(end), radius)) {
        TriangleLineHit hit{Vector3::new_(0.0f, 0.0f, 0.0f), Vector3::new_(0.0f, 0.0f, 0.0f), 0.0f, {0.0f, 0.0f, 0.0f}};
        if (triangle_segment(hit, s, delta, entry->triangle.vertices, radius, 0.0f)) {
            const float lower = -hit.fraction >= 0.0f ? 0.0f : hit.fraction;
            const float fraction = 1.0f - lower >= 0.0f ? lower : 1.0f;
            if (fraction < nearest) {
                nearest = fraction;
                if (index > std::size_t(std::numeric_limits<std::uint32_t>::max())) {
                    return rs::make_err(std::string("World geometry index overflow"));
                }
                result = SurfaceHit{
                    lanes(hit.position),
                    lanes(entry->triangle.feature.normal),
                    IDENTITY,
                    entry->tag,
                    static_cast<std::uint32_t>(index),
                };
            }
        }
    }
    return result;
}

rs::Result<std::vector<std::array<Vector, 3>>, std::string> nearby(const BoardWorld& world, Vector center,
                                                                  float radius) {
    if (!std::isfinite(radius) || radius < 0.0f) {
        return rs::make_err(std::string("Invalid nearby-trajectory triangle radius"));
    }
    Vector minimum{};
    Vector maximum{};
    for (std::size_t i = 0; i < 4; ++i) {
        minimum[i] = center[i] - radius;
        maximum[i] = center[i] + radius;
    }
    std::vector<std::array<Vector, 3>> triangles;
    triangles.reserve(64);
    for (const auto& [index, entry] : world.line_candidates(vec3(center), vec3(center), radius)) {
        (void)index;
        const auto& tv = entry->triangle.vertices;
        const std::array<Vector, 3> vertices{lanes(tv[0]), lanes(tv[1]), lanes(tv[2])};
        if (triangle_box(vertices, minimum, maximum)) {
            triangles.push_back(vertices);
            if (triangles.size() == 64) {
                break;
            }
        }
    }
    return triangles;
}

bool triangle_box(std::array<Vector, 3> vertices, Vector minimum, Vector maximum) {
    Vector center{};
    Vector half{};
    for (std::size_t i = 0; i < 4; ++i) center[i] = (minimum[i] + maximum[i]) * 0.5f;
    for (std::size_t i = 0; i < 4; ++i) half[i] = (maximum[i] - minimum[i]) * 0.5f;
    const std::array<Vector, 3> v{sub(vertices[0], center), sub(vertices[1], center), sub(vertices[2], center)};
    const std::array<Vector, 3> edges{sub(v[1], v[0]), sub(v[2], v[1]), sub(v[0], v[2])};
    const std::array<Vector, 3> axes{{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    }};
    for (const auto& edge : edges) {
        for (const auto& axis : axes) {
            if (separated(v, cross(edge, axis), half)) {
                return false;
            }
        }
    }
    for (const auto& axis : axes) {
        if (separated(v, axis, half)) {
            return false;
        }
    }
    return !separated(v, cross(edges[0], edges[1]), half);
}

bool separated(std::array<Vector, 3> vertices, Vector axis, Vector half) {
    const std::array<float, 3> projections{dot(vertices[0], axis), dot(vertices[1], axis), dot(vertices[2], axis)};
    const float min = rs::min(rs::min(projections[0], projections[1]), projections[2]);
    const float max = rs::max(rs::max(projections[0], projections[1]), projections[2]);
    const float radius =
        (half[0] * rs::abs(axis[0]) + half[1] * rs::abs(axis[1])) + half[2] * rs::abs(axis[2]);
    return min > radius || max < -radius;
}

float dot(Vector a, Vector b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }

Vector sub(Vector a, Vector b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }

Vector cross(Vector a, Vector b) {
    return {
        std::fma(-a[2], b[1], a[1] * b[2]),
        std::fma(-a[0], b[2], a[2] * b[0]),
        std::fma(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}

Vector3 vec3(Vector v) { return Vector3::new_(v[0], v[1], v[2]); }

Vector lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }

}  // namespace s3::game::physics::air_trajectory::world
