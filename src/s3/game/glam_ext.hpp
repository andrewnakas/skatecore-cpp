// glam 0.30.10 items used by physics/climbing and physics/network that stage A's
// animation_pose/glam.hpp does not provide. Same backend rules as that header (aarch64,
// default `std` features): Vec2/Vec3/Mat3 are the scalar structs (`src/f32/vec2.rs`,
// `vec3.rs`, `mat3.rs`); Vec4/Quat/Mat4 are the NEON backend (`src/f32/neon/*.rs`), lanes
// emulated with the stage A `neon::` helpers. No fusion anywhere (build -ffp-contract=off).
// `math::sin/sin_cos/sqrt` are `f32::sin/sin_cos/sqrt` (std math), `acos_approx` is glam's
// DirectXMath XMScalarAcos polynomial. glam_assert! is disabled (default features).
#pragma once
#include <array>
#include <cmath>
#include <optional>
#include <utility>

#include "s3/core/rs.hpp"
#include "s3/game/animation_pose/glam.hpp"

namespace s3::game::glam {

// ---- math.rs ------------------------------------------------------------------------
namespace math {
/// `acos_approx_f32`: a very close approximation of `v.clamp(-1.0, 1.0).acos()`.
inline float acos_approx(float v) {
    const bool nonnegative = v >= 0.0f;
    const float x = std::fabs(v);
    float omx = 1.0f - x;
    if (omx < 0.0f) omx = 0.0f;
    const float root = rs::sqrt(omx);
    float result =
        ((((((-0.001'262'491'1f * x + 0.006'670'09f) * x - 0.017'088'126f) * x + 0.030'891'88f) * x -
           0.050'174'303f) *
              x +
          0.088'978'99f) *
             x -
         0.214'598'8f) *
            x +
        1.570'796'3f;
    result *= root;
    if (nonnegative) return result;
    return rs::f32::consts::PI - result;
}
/// `f32::sin_cos`.
inline std::pair<float, float> sin_cos(float f) { return {std::sin(f), std::cos(f)}; }
}  // namespace math

// ---- Vec2 (scalar) ------------------------------------------------------------------
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    static constexpr Vec2 new_(float x, float y) { return Vec2{x, y}; }
    float dot(Vec2 rhs) const { return (x * rhs.x) + (y * rhs.y); }
    float length_squared() const { return dot(*this); }
    float length() const { return rs::sqrt(dot(*this)); }
};
S3_DERIVE_EQ(Vec2, x, y)

// ---- Vec3 (scalar) extras -----------------------------------------------------------
inline constexpr Vec3 VEC3_ZERO{0.0f, 0.0f, 0.0f};
inline constexpr Vec3 VEC3_ONE{1.0f, 1.0f, 1.0f};
inline constexpr Vec3 VEC3_X{1.0f, 0.0f, 0.0f};
inline constexpr Vec3 VEC3_Y{0.0f, 1.0f, 0.0f};
inline constexpr Vec3 VEC3_Z{0.0f, 0.0f, 1.0f};

inline Vec3 operator/(Vec3 a, float s) { return Vec3{a.x / s, a.y / s, a.z / s}; }
inline Vec3 operator*(float s, Vec3 a) { return Vec3{s * a.x, s * a.y, s * a.z}; }

inline float length_squared(Vec3 v) { return v.dot(v); }
inline float length(Vec3 v) { return rs::sqrt(v.dot(v)); }
inline float length_recip(Vec3 v) { return 1.0f / length(v); }
inline float distance(Vec3 a, Vec3 b) { return length(a - b); }
inline float distance_squared(Vec3 a, Vec3 b) { return length_squared(a - b); }
inline Vec3 lerp(Vec3 a, Vec3 rhs, float s) { return a * (1.0f - s) + rhs * s; }
inline Vec3 cross(Vec3 a, Vec3 rhs) {
    return Vec3{a.y * rhs.z - rhs.y * a.z, a.z * rhs.x - rhs.z * a.x, a.x * rhs.y - rhs.x * a.y};
}
inline Vec3 normalize(Vec3 v) { return v * length_recip(v); }
inline std::optional<Vec3> try_normalize(Vec3 v) {
    const float rcp = length_recip(v);
    if (std::isfinite(rcp) && rcp > 0.0f) return v * rcp;
    return std::nullopt;
}
inline Vec3 normalize_or(Vec3 v, Vec3 fallback) {
    const float rcp = length_recip(v);
    if (std::isfinite(rcp) && rcp > 0.0f) return v * rcp;
    return fallback;
}
inline Vec3 normalize_or_zero(Vec3 v) { return normalize_or(v, VEC3_ZERO); }
/// Pixar orthonormal basis (`math::signum` = f32::signum).
inline Vec3 any_orthonormal_vector(Vec3 v) {
    const float sign = rs::signum(v.z);
    const float a = -1.0f / (sign + v.z);
    const float b = v.x * v.y * a;
    return Vec3{b, sign + v.y * v.y * a, -v.y};
}

// ---- Vec4 (NEON) extras -------------------------------------------------------------
inline Vec3 truncate(Vec4 v) { return v.xyz(); }
inline float length_squared(Vec4 v) { return v.dot(v); }

// ---- Quat (NEON) extras -------------------------------------------------------------
inline Quat operator*(Quat q, float s) { return Quat::from_vec4(q.as_vec4() * s); }
inline Quat operator+(Quat a, Quat b) { return Quat::from_vec4(a.as_vec4() + b.as_vec4()); }
inline Quat operator-(Quat q) { return q * -1.0f; }

inline float length_squared(Quat q) { return q.as_vec4().dot(q.as_vec4()); }
inline Quat quat_from_axis_angle(Vec3 axis, float angle) {
    const auto [s, c] = math::sin_cos(angle * 0.5f);
    const Vec3 v = axis * s;
    return Quat::from_xyzw(v.x, v.y, v.z, c);
}
inline Quat quat_from_rotation_y(float angle) {
    const auto [s, c] = math::sin_cos(angle * 0.5f);
    return Quat::from_xyzw(0.0f, s, 0.0f, c);
}
inline Quat quat_from_rotation_arc(Vec3 from, Vec3 to) {
    const float ONE_MINUS_EPS = 1.0f - 2.0f * rs::f32::EPSILON;
    const float dot = from.dot(to);
    if (dot > ONE_MINUS_EPS) return Quat::IDENTITY;
    if (dot < -ONE_MINUS_EPS) return quat_from_axis_angle(any_orthonormal_vector(from), rs::f32::consts::PI);
    const Vec3 c = cross(from, to);
    return Quat::from_xyzw(c.x, c.y, c.z, 1.0f + dot).normalize();
}
/// `Quat::lerp_impl`.
inline Quat quat_lerp_impl(Quat a, Quat end, float s) { return (a * (1.0f - s) + end * s).normalize(); }
inline Quat slerp(Quat a, Quat end, float s) {
    float dot = a.dot(end);
    if (dot < 0.0f) {
        end = -end;
        dot = -dot;
    }
    const float DOT_THRESHOLD = 1.0f - rs::f32::EPSILON;
    if (dot > DOT_THRESHOLD) return quat_lerp_impl(a, end, s);
    const float theta = math::acos_approx(dot);
    const float scale1 = std::sin(theta * (1.0f - s));
    const float scale2 = std::sin(theta * s);
    const float theta_sin = std::sin(theta);
    return ((a * scale1) + (end * scale2)) * (1.0f / theta_sin);
}

// ---- Mat3 (scalar Vec3 columns) -----------------------------------------------------
struct Mat3 {
    Vec3 x_axis;
    Vec3 y_axis;
    Vec3 z_axis;

    static constexpr Mat3 from_cols(Vec3 x, Vec3 y, Vec3 z) { return Mat3{x, y, z}; }
    static constexpr Mat3 from_cols_array_2d(const std::array<std::array<float, 3>, 3>& m) {
        return Mat3{Vec3{m[0][0], m[0][1], m[0][2]}, Vec3{m[1][0], m[1][1], m[1][2]},
                    Vec3{m[2][0], m[2][1], m[2][2]}};
    }
    std::array<std::array<float, 3>, 3> to_cols_array_2d() const {
        return {x_axis.to_array(), y_axis.to_array(), z_axis.to_array()};
    }
    static Mat3 from_mat4(const Mat4& m) { return Mat3{m.x_axis.xyz(), m.y_axis.xyz(), m.z_axis.xyz()}; }
    static Mat3 from_quat(Quat rotation) {
        const float x2 = rotation.x + rotation.x;
        const float y2 = rotation.y + rotation.y;
        const float z2 = rotation.z + rotation.z;
        const float xx = rotation.x * x2;
        const float xy = rotation.x * y2;
        const float xz = rotation.x * z2;
        const float yy = rotation.y * y2;
        const float yz = rotation.y * z2;
        const float zz = rotation.z * z2;
        const float wx = rotation.w * x2;
        const float wy = rotation.w * y2;
        const float wz = rotation.w * z2;
        return from_cols(Vec3{1.0f - (yy + zz), xy + wz, xz - wy}, Vec3{xy - wz, 1.0f - (xx + zz), yz + wx},
                         Vec3{xz + wy, yz - wx, 1.0f - (xx + yy)});
    }
    Vec3 mul_vec3(Vec3 rhs) const {
        Vec3 res = x_axis * rhs.x;
        res = res + y_axis * rhs.y;
        res = res + z_axis * rhs.z;
        return res;
    }
    Mat3 mul_mat3(const Mat3& rhs) const {
        return from_cols(mul_vec3(rhs.x_axis), mul_vec3(rhs.y_axis), mul_vec3(rhs.z_axis));
    }
    friend Mat3 operator*(const Mat3& a, const Mat3& b) { return a.mul_mat3(b); }
    friend Vec3 operator*(const Mat3& a, Vec3 b) { return a.mul_vec3(b); }
};
S3_DERIVE_EQ(Mat3, x_axis, y_axis, z_axis)

/// `Quat::from_mat3` (scalar from_rotation_axes).
inline Quat quat_from_mat3(const Mat3& m) { return Quat::from_rotation_axes(m.x_axis, m.y_axis, m.z_axis); }

// ---- Mat4 (NEON) extras -------------------------------------------------------------
inline Vec3 transform_point3(const Mat4& m, Vec3 rhs) {
    Vec4 res = m.x_axis * rhs.x;
    res = m.y_axis * rhs.y + res;
    res = m.z_axis * rhs.z + res;
    res = m.w_axis + res;
    return res.xyz();
}
inline Vec3 transform_vector3(const Mat4& m, Vec3 rhs) {
    Vec4 res = m.x_axis * rhs.x;
    res = m.y_axis * rhs.y + res;
    res = m.z_axis * rhs.z + res;
    return res.xyz();
}
inline Mat4 mat4_from_rotation_translation(Quat rotation, Vec3 translation) {
    const auto [x_axis, y_axis, z_axis] = Mat4::quat_to_axes(rotation);
    return Mat4::from_cols(x_axis, y_axis, z_axis, translation.extend(1.0f));
}
inline std::array<std::array<float, 4>, 4> to_cols_array_2d(const Mat4& m) {
    return {m.x_axis.to_array(), m.y_axis.to_array(), m.z_axis.to_array(), m.w_axis.to_array()};
}

}  // namespace s3::game::glam
