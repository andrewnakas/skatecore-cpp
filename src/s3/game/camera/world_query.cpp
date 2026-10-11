// Port of simref sim/src/camera/world_query.rs.
#include "s3/game/camera/world_query.hpp"

#include "s3/core/physics/triangle_query.hpp"

namespace s3::game::camera::world_query {

using s3::camera::F4;
using s3::camera::FatLineResult;
using s3::math::Vector3;

namespace {
Vector3 vector(F4 v) { return Vector3::new_(v[0], v[1], v[2]); }
F4 lanes(Vector3 v) { return {v.x, v.y, v.z, 0.0f}; }
}  // namespace

rs::Result<FatLineResult, std::string> line(const s3::physics::board_world::BoardWorld& world, F4 start_lanes, F4 end,
                                            float radius) {
    if (!rs::is_finite(radius) || radius < 0.0f) {
        return rs::make_err(std::string("Invalid stock camera collision radius"));
    }
    const Vector3 start = vector(start_lanes);
    const Vector3 delta = Vector3::new_(end[0] - start.x, end[1] - start.y, end[2] - start.z);
    float fraction = rs::f32::MAX;
    FatLineResult result = no_hit();
    for (const auto& candidate : world.line_candidates(start, vector(end), radius)) {
        const auto& entry = *candidate.second;
        s3::physics::triangle_query::TriangleLineHit hit{};
        hit.position = Vector3::ZERO;
        hit.normal = Vector3::ZERO;
        hit.fraction = 0.0f;
        hit.volume_parameter = {0.0f, 0.0f, 0.0f};
        // Zero triangle fatness and zero material exclusion mask are native.
        // Subject actor exclusion is preserved by BoardWorld's world-only data.
        if (s3::physics::triangle_query::triangle_segment(hit, start, delta, entry.triangle.vertices, radius, 0.0f)) {
            const float lower = -hit.fraction >= 0.0f ? 0.0f : hit.fraction;
            const float c = 1.0f - lower >= 0.0f ? lower : 1.0f;
            if (c < fraction) {
                fraction = c;
                result = FatLineResult{lanes(hit.position), lanes(entry.triangle.feature.normal), c, 1, entry.tag};
            }
        }
    }
    return result;
}

FatLineResult no_hit() { return FatLineResult{F4{}, F4{}, 0.0f, 0, 0}; }

}  // namespace s3::game::camera::world_query
