// Port of skate-game/src/physics/known_air/math.rs.
//! Original KnownAir VMX geometry; independent PC estimate seeds.
#include "s3/game/physics/known_air/math.hpp"

#include "s3/core/riding/collision_response.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/known_air/runtime.hpp"

namespace s3::game::physics::known_air::math {

float dot(V a, V b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }

V sub(V a, V b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }

V scale(V a, float s) { return {a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }

V cross(V a, V b) {
    return {
        std::fma(-a[2], b[1], a[1] * b[2]),
        std::fma(-a[0], b[2], a[2] * b[0]),
        std::fma(-a[1], b[0], a[0] * b[1]),
        std::fma(-a[3], b[3], a[3] * b[3]),
    };
}

std::pair<V, float> normalized(V v) {
    const float s = dot(v, v);
    float r = rs::recip(rs::sqrt(s));
    for (int i = 0; i < 2; ++i) {
        r = std::fma(r * 0.5f, std::fma(-s, r * r, 1.0f), r);
    }
    const float l = s == 0.0f ? 0.0f : s * r;
    return {l > rs::from_bits(0x358637bdu) ? scale(v, r) : V{0.0f, 0.0f, 0.0f, 0.0f}, l};
}

V rotate(const std::array<V, 4>& m, V v) {
    V out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = m[0][i] * v[0];
        const float y = std::fma(m[1][i], v[1], x);
        out[i] = std::fma(m[2][i], v[2], y);
    }
    return out;
}

V point(const std::array<V, 4>& m, V v) {
    V out{};
    for (std::size_t i = 0; i < 4; ++i) {
        const float x = std::fma(m[0][i], v[0], m[3][i]);
        const float y = std::fma(m[1][i], v[1], x);
        out[i] = std::fma(m[2][i], v[2], y);
    }
    return out;
}

::s3::math::Vector3 xyz(V v) { return ::s3::math::Vector3::new_(v[0], v[1], v[2]); }

}  // namespace s3::game::physics::known_air::math

namespace s3::game::physics::known_air::runtime {

using namespace ::s3::game::physics::known_air::math;

float Runtime::dot3(V a, V b) { return dot(a, b); }

V Runtime::divide_vector_by_scalar(V v, float s) {
    float r = rs::recip(s);
    for (int i = 0; i < 2; ++i) {
        r = std::fma(std::fma(-s, r, 1.0f), r, r);
    }
    return scale(v, r);
}

std::pair<V, float> Runtime::normalize_safe_with_length(V v) { return normalized(v); }

float Runtime::signed_angle_about_axis(V a, V b, V axis) {
    if (!(dot(a, a) * dot(b, b) > rs::from_bits(0x37800000u))) {
        return 0.0f;
    }
    const V a2 = sub(a, scale(axis, dot(axis, a)));
    const V b2 = sub(b, scale(axis, dot(axis, b)));
    return ::s3::riding::collision_response::signed_angle(xyz(a2), xyz(b2), xyz(axis));
}

float Runtime::wrap_angle_8258db98(float a) {
    const float pi = rs::from_bits(0x40490fdbu);
    const float tau = rs::from_bits(0x40c90fdbu);
    if (a < -pi || !(a < pi)) {
        const std::int32_t n = rs::as_i32(a * rs::from_bits(0x3e22f983u));
        a = std::fma(-float(n), tau, a);
        if (!(a < pi)) {
            a -= tau;
        } else if (a < -pi) {
            a += tau;
        }
    }
    return a;
}

RestoreVelocityGeometry Runtime::restore_velocity_geometry_82d35998(V v, V n) {
    const V tangent = sub(v, scale(n, dot(n, v)));
    const V downhill = cross(n, cross(n, V{0.0f, 1.0f, 0.0f, 0.0f}));
    RestoreVelocityGeometry g;
    g.tangential_velocity = tangent;
    g.landing_speed_curve_input = n[1];
    g.landing_speed_blend_source = dot(normalized(downhill).first, normalized(tangent).first);
    return g;
}

V Runtime::transform_point_82d36880(AffineTransform m, V p) { return point(m.vectors, p); }

}  // namespace s3::game::physics::known_air::runtime
