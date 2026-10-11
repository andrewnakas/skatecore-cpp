// Port of skate-core/src/physics/skeleton_body/collision_filter.rs.
// Original82BD5A00: select the angular interval of active contact planes.
#include "s3/core/physics/skeleton_body.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::skeleton_body::collision_feedback {

using namespace collision_vector;
using riding::collision_response::signed_angle;

namespace {
math::Vector3 vector(V v) { return math::Vector3{v[0], v[1], v[2]}; }
}  // namespace

V SkeletonCollisionFeedback::filter_error(V error, V axis) const {
    const V projected = sub(error, scale(axis, dot(error, axis)));
    const V direction = normalize(projected);
    if (dot(direction, direction) < 0.1f) {
        return projected;
    }
    const float half_pi = rs::from_bits(0x3fc90fdbu);
    float minimum = half_pi;
    float maximum = -half_pi;
    bool found = false;
    for (const auto& plane : planes) {
        const V normal = normalize(sub(plane.normal, scale(axis, dot(plane.normal, axis))));
        if (dot(normal, normal) < 0.1f) {
            continue;
        }
        const float angle = signed_angle(vector(direction), vector(normal), vector(axis));
        const float turns = angle * rs::from_bits(0x3e22f983u);
        const float fractional = turns - rs::floor(turns);
        const float centered = fractional - (fractional > 0.5f ? 1.0f : 0.0f);
        const float wrapped = centered * rs::from_bits(0x40c90fdbu);
        if (wrapped > -half_pi && wrapped < half_pi) {
            if (wrapped < minimum) {
                minimum = wrapped;
            }
            if (wrapped > maximum) {
                maximum = wrapped;
            }
            found = true;
        }
    }
    const float angle = (maximum + minimum) * 0.5f;
    const float magnitude = found ? trigonometry::cos(angle) : 0.0f;
    //82BD35B8 rotates identity around the supplied axis before applying
    //the projected error. Preserve its product/FMA and standalone trig.
    const float c = trigonometry::cos(angle);
    const float s = trigonometry::sin(angle);
    const float x = axis[0];
    const float y = axis[1];
    const float z = axis[2];
    const float sx = s * x;
    const float sy = s * y;
    const float sz = s * z;
    const float t = 1.0f - c;
    const float tx = t * x;
    const float ty = t * y;
    const float tz = t * z;
    const float rotation[3][3] = {
        {rs::mul_add(x, tx, c), rs::mul_add(tx, y, sz), tx * z - sy},
        {ty * x - sz, rs::mul_add(ty, y, c), rs::mul_add(ty, z, sx)},
        {rs::mul_add(tz, x, sy), tz * y - sx, rs::mul_add(z, tz, c)},
    };
    V result{0.0f, 0.0f, 0.0f, 0.0f};
    for (std::size_t i = 0; i < 3; ++i) {
        result[i] = rs::mul_add(rotation[2][i], projected[2],
                                rs::mul_add(rotation[1][i], projected[1], rotation[0][i] * projected[0])) *
                    magnitude;
    }
    return result;
}

}  // namespace s3::physics::skeleton_body::collision_feedback
