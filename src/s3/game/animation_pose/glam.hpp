// Minimal port of the glam 0.30.10 items used by skate-game (animation.rs native_matrix and
// animation_pose/authored_clips.rs): Vec3, Vec4, Quat, Mat4.
//
// Mirrored implementation: the reference simulation (simref) runs on aarch64 with glam's
// default features, where `glam::f32` selects its NEON backend (`src/f32/neon/*.rs`;
// lib.rs cfg `target_arch = "aarch64"`, no `scalar-math`/`core-simd`). Vec3 is always the
// scalar struct (`src/f32/vec3.rs`). Every lane operation below reproduces the exact
// intrinsic sequence of that backend, emulated lane-by-lane so the port is portable (wasm):
//   - vmulq/vaddq/vsubq/vmulq_n lanes are independent binary32 ops. No fusion: Rust never
//     sets LLVM `contract`, and `vmlsq_f32` in stdarch is `simd_sub(a, simd_mul(b, c))`
//     (not FMLS), so Vec3A::cross is an unfused `a*b - c*d` per lane.
//   - `vaddvq_f32` (dot4) lowers to FADDP.4S + FADDP.2S: (p0 + p1) + (p2 + p3).
//   - Vec4::length = sqrt(dot4); normalize multiplies by `1.0 / length`.
//   - Mat4::inverse is the glm-derived NEON swizzle algorithm (uzp/trn/zip lanes reproduced),
//     Mat4::determinant/quat_to_axes/Quat::from_rotation_axes are scalar in glam.
//   - Quat::mul_quat is the NEON rev64/ext/sign-control sequence; Quat * Vec3 goes through
//     mul_vec3a with scalar Vec3A dots ((x*x + y*y) + z*z).
//   - glam_assert! is disabled (default features), so no assertions here.
// Build with -ffp-contract=off (the C++ side must not fuse either).
#pragma once
#include <array>
#include <cmath>
#include <tuple>

#include "s3/core/rs.hpp"

namespace s3::game::glam {

namespace neon {
/// One `float32x4_t`, lanes 0..3.
using F4 = std::array<float, 4>;
inline F4 vmulq_f32(F4 a, F4 b) { return {a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]}; }
inline F4 vaddq_f32(F4 a, F4 b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2], a[3] + b[3]}; }
inline F4 vsubq_f32(F4 a, F4 b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
inline F4 vmulq_n_f32(F4 a, float s) { return {a[0] * s, a[1] * s, a[2] * s, a[3] * s}; }
inline F4 vuzp1q_f32(F4 a, F4 b) { return {a[0], a[2], b[0], b[2]}; }
inline F4 vuzp2q_f32(F4 a, F4 b) { return {a[1], a[3], b[1], b[3]}; }
inline F4 vtrn1q_f32(F4 a, F4 b) { return {a[0], b[0], a[2], b[2]}; }
inline F4 vtrn2q_f32(F4 a, F4 b) { return {a[1], b[1], a[3], b[3]}; }
inline F4 vzip1q_f32(F4 a, F4 b) { return {a[0], b[0], a[1], b[1]}; }
inline F4 vzip2q_f32(F4 a, F4 b) { return {a[2], b[2], a[3], b[3]}; }
inline F4 vrev64q_f32(F4 a) { return {a[1], a[0], a[3], a[2]}; }
/// `vextq_f32(a, b, n)`: lanes n.. of the concatenation a:b.
inline F4 vextq_f32(F4 a, F4 b, int n) {
    const float c[8] = {a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]};
    return {c[n], c[n + 1], c[n + 2], c[n + 3]};
}
inline F4 vdupq_laneq_f32(F4 a, int i) { return {a[i], a[i], a[i], a[i]}; }
inline F4 vsetq_lane_f32(float v, F4 a, int i) {
    a[i] = v;
    return a;
}
/// FADDP pairwise reduction order of `vaddvq_f32`.
inline float vaddvq_f32(F4 a) { return (a[0] + a[1]) + (a[2] + a[3]); }
/// `neon::dot4`.
inline float dot4(F4 lhs, F4 rhs) { return vaddvq_f32(vmulq_f32(lhs, rhs)); }
}  // namespace neon

struct Vec4;

/// `glam::Vec3` (scalar struct on every backend).
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    static constexpr Vec3 new_(float x, float y, float z) { return Vec3{x, y, z}; }
    static constexpr Vec3 from_array(std::array<float, 3> a) { return Vec3{a[0], a[1], a[2]}; }
    /// `Vec3::from_slice` (first three elements; Rust panics on a shorter slice).
    template <class C>
    static Vec3 from_slice(const C& s) {
        if (s.size() < 3) S3_PANIC("Vec3::from_slice: slice too short");
        return Vec3{s[0], s[1], s[2]};
    }
    std::array<float, 3> to_array() const { return {x, y, z}; }
    inline Vec4 extend(float w) const;
    Vec3 recip() const { return Vec3{1.0f / x, 1.0f / y, 1.0f / z}; }
    float min_element() const {
        auto min = [](float a, float b) { return a < b ? a : b; };
        return min(x, min(y, z));
    }
    float max_element() const {
        auto max = [](float a, float b) { return a > b ? a : b; };
        return max(x, max(y, z));
    }
    bool is_finite() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
    float dot(Vec3 rhs) const { return (x * rhs.x) + (y * rhs.y) + (z * rhs.z); }

    friend Vec3 operator+(Vec3 a, Vec3 b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend Vec3 operator-(Vec3 a, Vec3 b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend Vec3 operator*(Vec3 a, Vec3 b) { return Vec3{a.x * b.x, a.y * b.y, a.z * b.z}; }
    friend Vec3 operator/(Vec3 a, Vec3 b) { return Vec3{a.x / b.x, a.y / b.y, a.z / b.z}; }
    friend Vec3 operator*(Vec3 a, float s) { return Vec3{a.x * s, a.y * s, a.z * s}; }
    friend Vec3 operator-(Vec3 a) { return Vec3{-a.x, -a.y, -a.z}; }
};
S3_DERIVE_EQ(Vec3, x, y, z)

/// `glam::Vec4` (NEON `float32x4_t`).
struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    static constexpr Vec4 new_(float x, float y, float z, float w) { return Vec4{x, y, z, w}; }
    static Vec4 from_f4(neon::F4 v) { return Vec4{v[0], v[1], v[2], v[3]}; }
    neon::F4 f4() const { return {x, y, z, w}; }
    std::array<float, 4> to_array() const { return {x, y, z, w}; }
    Vec3 xyz() const { return Vec3{x, y, z}; }
    float dot(Vec4 rhs) const { return neon::dot4(f4(), rhs.f4()); }
    float length() const { return rs::sqrt(dot(*this)); }
    float length_recip() const { return 1.0f / length(); }
    Vec4 normalize() const { return *this * length_recip(); }
    /// `vcltq_f32(vabsq_f32(v), INFINITY)` on every lane.
    bool is_finite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w);
    }
    friend Vec4 operator*(Vec4 a, float s) { return from_f4(neon::vmulq_n_f32(a.f4(), s)); }
    friend Vec4 operator*(Vec4 a, Vec4 b) { return from_f4(neon::vmulq_f32(a.f4(), b.f4())); }
    friend Vec4 operator+(Vec4 a, Vec4 b) { return from_f4(neon::vaddq_f32(a.f4(), b.f4())); }
    friend Vec4 operator-(Vec4 a, Vec4 b) { return from_f4(neon::vsubq_f32(a.f4(), b.f4())); }
};
S3_DERIVE_EQ(Vec4, x, y, z, w)

inline Vec4 Vec3::extend(float w) const { return Vec4{x, y, z, w}; }

/// `glam::Quat` (NEON `float32x4_t`, x,y,z,w).
struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    static const Quat IDENTITY;
    static constexpr Quat from_xyzw(float x, float y, float z, float w) { return Quat{x, y, z, w}; }
    static constexpr Quat from_array(std::array<float, 4> a) { return Quat{a[0], a[1], a[2], a[3]}; }
    static constexpr Quat from_vec4(Vec4 v) { return Quat{v.x, v.y, v.z, v.w}; }
    std::array<float, 4> to_array() const { return {x, y, z, w}; }
    Vec4 as_vec4() const { return Vec4{x, y, z, w}; }
    neon::F4 f4() const { return {x, y, z, w}; }
    static Quat from_f4(neon::F4 v) { return Quat{v[0], v[1], v[2], v[3]}; }

    /// DirectXMath `XMQuaternionRotationMatrix` (scalar in glam).
    static Quat from_rotation_axes(Vec3 x_axis, Vec3 y_axis, Vec3 z_axis) {
        const float m00 = x_axis.x, m01 = x_axis.y, m02 = x_axis.z;
        const float m10 = y_axis.x, m11 = y_axis.y, m12 = y_axis.z;
        const float m20 = z_axis.x, m21 = z_axis.y, m22 = z_axis.z;
        if (m22 <= 0.0f) {
            const float dif10 = m11 - m00;
            const float omm22 = 1.0f - m22;
            if (dif10 <= 0.0f) {
                const float four_xsq = omm22 - dif10;
                const float inv4x = 0.5f / rs::sqrt(four_xsq);
                return from_xyzw(four_xsq * inv4x, (m01 + m10) * inv4x, (m02 + m20) * inv4x,
                                 (m12 - m21) * inv4x);
            }
            const float four_ysq = omm22 + dif10;
            const float inv4y = 0.5f / rs::sqrt(four_ysq);
            return from_xyzw((m01 + m10) * inv4y, four_ysq * inv4y, (m12 + m21) * inv4y,
                             (m20 - m02) * inv4y);
        }
        const float sum10 = m11 + m00;
        const float opm22 = 1.0f + m22;
        if (sum10 <= 0.0f) {
            const float four_zsq = opm22 - sum10;
            const float inv4z = 0.5f / rs::sqrt(four_zsq);
            return from_xyzw((m02 + m20) * inv4z, (m12 + m21) * inv4z, four_zsq * inv4z,
                             (m01 - m10) * inv4z);
        }
        const float four_wsq = opm22 + sum10;
        const float inv4w = 0.5f / rs::sqrt(four_wsq);
        return from_xyzw((m12 - m21) * inv4w, (m20 - m02) * inv4w, (m01 - m10) * inv4w,
                         four_wsq * inv4w);
    }

    Quat conjugate() const {
        return from_f4(neon::vmulq_f32(f4(), neon::F4{-1.0f, -1.0f, -1.0f, 1.0f}));
    }
    Quat inverse() const { return conjugate(); }
    float dot(Quat rhs) const { return as_vec4().dot(rhs.as_vec4()); }
    float length() const { return as_vec4().length(); }
    Quat normalize() const { return from_vec4(as_vec4().normalize()); }
    bool is_finite() const { return as_vec4().is_finite(); }

    Quat mul_quat(Quat rhs_q) const {
        using namespace neon;
        const F4 lhs = f4();
        const F4 rhs = rhs_q.f4();
        const F4 CONTROL_WZYX = {1.0f, -1.0f, 1.0f, -1.0f};
        const F4 CONTROL_ZWXY = {1.0f, 1.0f, -1.0f, -1.0f};
        const F4 CONTROL_YXWZ = {-1.0f, 1.0f, 1.0f, -1.0f};

        const F4 r_xxxx = vdupq_laneq_f32(lhs, 0);
        const F4 r_yyyy = vdupq_laneq_f32(lhs, 1);
        const F4 r_zzzz = vdupq_laneq_f32(lhs, 2);
        const F4 r_wwww = vdupq_laneq_f32(lhs, 3);

        const F4 lxrw_lyrw_lzrw_lwrw = vmulq_f32(r_wwww, rhs);
        F4 l_wzyx = vrev64q_f32(rhs);
        l_wzyx = vextq_f32(l_wzyx, l_wzyx, 2);

        const F4 lwrx_lzrx_lyrx_lxrx = vmulq_f32(r_xxxx, l_wzyx);
        const F4 l_zwxy = vrev64q_f32(l_wzyx);

        const F4 lwrx_nlzrx_lyrx_nlxrx = vmulq_f32(lwrx_lzrx_lyrx_lxrx, CONTROL_WZYX);

        const F4 lzry_lwry_lxry_lyry = vmulq_f32(r_yyyy, l_zwxy);
        F4 l_yxwz = vrev64q_f32(l_zwxy);
        l_yxwz = vextq_f32(l_yxwz, l_yxwz, 2);

        const F4 lzry_lwry_nlxry_nlyry = vmulq_f32(lzry_lwry_lxry_lyry, CONTROL_ZWXY);

        const F4 lyrz_lxrz_lwrz_lzrz = vmulq_f32(r_zzzz, l_yxwz);
        const F4 result0 = vaddq_f32(lxrw_lyrw_lzrw_lwrw, lwrx_nlzrx_lyrx_nlxrx);

        const F4 nlyrz_lxrz_lwrz_wlzrz = vmulq_f32(lyrz_lxrz_lwrz_lzrz, CONTROL_YXWZ);
        const F4 result1 = vaddq_f32(lzry_lwry_nlxry_nlyry, nlyrz_lxrz_lwrz_wlzrz);
        return from_f4(vaddq_f32(result0, result1));
    }

    /// `Quat::mul_vec3a` (Vec3 converts through Vec3A).
    Vec3 mul_vec3(Vec3 rhs) const {
        using namespace neon;
        const Vec3 b{x, y, z};
        const float b2 = b.dot(b);  // Vec3A::length_squared: scalar dot
        // Vec3A::cross(b, rhs): part_a - (a.zxy * b.yzx), unfused per lane.
        const F4 cross = {b.y * rhs.z - b.z * rhs.y, b.z * rhs.x - b.x * rhs.z,
                          b.x * rhs.y - b.y * rhs.x, 0.0f};
        const F4 r = {rhs.x, rhs.y, rhs.z, 0.0f};
        const F4 bv = {b.x, b.y, b.z, 0.0f};
        const F4 out = vaddq_f32(vaddq_f32(vmulq_n_f32(r, (w * w) - b2), vmulq_n_f32(bv, rhs.dot(b) * 2.0f)),
                                 vmulq_n_f32(cross, w * 2.0f));
        return Vec3{out[0], out[1], out[2]};
    }

    friend Quat operator*(Quat a, Quat b) { return a.mul_quat(b); }
    friend Vec3 operator*(Quat a, Vec3 b) { return a.mul_vec3(b); }
};
inline constexpr Quat Quat::IDENTITY = Quat{0.0f, 0.0f, 0.0f, 1.0f};
S3_DERIVE_EQ(Quat, x, y, z, w)

/// `glam::Mat4`, column major (NEON backend).
struct Mat4 {
    Vec4 x_axis;
    Vec4 y_axis;
    Vec4 z_axis;
    Vec4 w_axis;

    static const Mat4 IDENTITY;

    static constexpr Mat4 from_cols(Vec4 x_axis, Vec4 y_axis, Vec4 z_axis, Vec4 w_axis) {
        return Mat4{x_axis, y_axis, z_axis, w_axis};
    }
    static constexpr Mat4 from_cols_array(const std::array<float, 16>& m) {
        return Mat4{Vec4{m[0], m[1], m[2], m[3]}, Vec4{m[4], m[5], m[6], m[7]},
                    Vec4{m[8], m[9], m[10], m[11]}, Vec4{m[12], m[13], m[14], m[15]}};
    }
    std::array<float, 16> to_cols_array() const {
        return {x_axis.x, x_axis.y, x_axis.z, x_axis.w, y_axis.x, y_axis.y, y_axis.z, y_axis.w,
                z_axis.x, z_axis.y, z_axis.z, z_axis.w, w_axis.x, w_axis.y, w_axis.z, w_axis.w};
    }

    static std::tuple<Vec4, Vec4, Vec4> quat_to_axes(Quat rotation) {
        const float x = rotation.x, y = rotation.y, z = rotation.z, w = rotation.w;
        const float x2 = x + x;
        const float y2 = y + y;
        const float z2 = z + z;
        const float xx = x * x2;
        const float xy = x * y2;
        const float xz = x * z2;
        const float yy = y * y2;
        const float yz = y * z2;
        const float zz = z * z2;
        const float wx = w * x2;
        const float wy = w * y2;
        const float wz = w * z2;
        return {Vec4{1.0f - (yy + zz), xy + wz, xz - wy, 0.0f},
                Vec4{xy - wz, 1.0f - (xx + zz), yz + wx, 0.0f},
                Vec4{xz + wy, yz - wx, 1.0f - (xx + yy), 0.0f}};
    }

    static Mat4 from_scale_rotation_translation(Vec3 scale, Quat rotation, Vec3 translation) {
        const auto [x_axis, y_axis, z_axis] = quat_to_axes(rotation);
        return from_cols(x_axis * scale.x, y_axis * scale.y, z_axis * scale.z,
                         translation.extend(1.0f));
    }

    std::tuple<Vec3, Quat, Vec3> to_scale_rotation_translation() const {
        const float det = determinant();
        const Vec3 scale = Vec3{x_axis.length() * rs::signum(det), y_axis.length(), z_axis.length()};
        const Vec3 inv_scale = scale.recip();
        const Quat rotation = Quat::from_rotation_axes(
            (x_axis * inv_scale.x).xyz(), (y_axis * inv_scale.y).xyz(), (z_axis * inv_scale.z).xyz());
        const Vec3 translation = w_axis.xyz();
        return {scale, rotation, translation};
    }

    bool is_finite() const {
        return x_axis.is_finite() && y_axis.is_finite() && z_axis.is_finite() && w_axis.is_finite();
    }

    float determinant() const {
        const float m00 = x_axis.x, m01 = x_axis.y, m02 = x_axis.z, m03 = x_axis.w;
        const float m10 = y_axis.x, m11 = y_axis.y, m12 = y_axis.z, m13 = y_axis.w;
        const float m20 = z_axis.x, m21 = z_axis.y, m22 = z_axis.z, m23 = z_axis.w;
        const float m30 = w_axis.x, m31 = w_axis.y, m32 = w_axis.z, m33 = w_axis.w;

        const float a2323 = m22 * m33 - m23 * m32;
        const float a1323 = m21 * m33 - m23 * m31;
        const float a1223 = m21 * m32 - m22 * m31;
        const float a0323 = m20 * m33 - m23 * m30;
        const float a0223 = m20 * m32 - m22 * m30;
        const float a0123 = m20 * m31 - m21 * m30;

        return m00 * (m11 * a2323 - m12 * a1323 + m13 * a1223) -
               m01 * (m10 * a2323 - m12 * a0323 + m13 * a0223) +
               m02 * (m10 * a1323 - m11 * a0323 + m13 * a0123) -
               m03 * (m10 * a1223 - m11 * a0223 + m12 * a0123);
    }

    /// NEON `Mat4::inverse` (glm `glm_mat4_inverse`).
    Mat4 inverse() const {
        using namespace neon;
        auto swizzle3377 = [](F4 a, F4 b) {
            const F4 r = vuzp2q_f32(a, b);
            return vtrn2q_f32(r, r);
        };
        auto swizzle2266 = [](F4 a, F4 b) {
            const F4 r = vuzp1q_f32(a, b);
            return vtrn2q_f32(r, r);
        };
        auto swizzle0046 = [](F4 a, F4 b) {
            const F4 r = vuzp1q_f32(a, a);
            return vuzp1q_f32(r, b);
        };
        auto swizzle1155 = [](F4 a, F4 b) {
            const F4 r = vzip1q_f32(a, b);
            return vzip2q_f32(r, r);
        };
        auto swizzle0044 = [](F4 a, F4 b) {
            const F4 r = vuzp1q_f32(a, b);
            return vtrn1q_f32(r, r);
        };
        auto swizzle0266 = [](F4 a, F4 b) {
            const F4 r = vuzp1q_f32(a, b);
            return vsetq_lane_f32(b[2], r, 2);
        };
        auto swizzle0246 = [](F4 a, F4 b) { return vuzp1q_f32(a, b); };
        const F4 x = x_axis.f4(), y = y_axis.f4(), z = z_axis.f4(), w = w_axis.f4();
        auto fac = [&](F4 swp0a, F4 swp0b, F4 swp00, F4 swp03) {
            const F4 swp01 = swizzle0046(swp0a, swp0a);
            const F4 swp02 = swizzle0046(swp0b, swp0b);
            const F4 mul00 = vmulq_f32(swp00, swp01);
            const F4 mul01 = vmulq_f32(swp02, swp03);
            return vsubq_f32(mul00, mul01);
        };
        const F4 fac0 = fac(swizzle3377(w, z), swizzle2266(w, z), swizzle2266(z, y), swizzle3377(z, y));
        const F4 fac1 = fac(swizzle3377(w, z), swizzle1155(w, z), swizzle1155(z, y), swizzle3377(z, y));
        const F4 fac2 = fac(swizzle2266(w, z), swizzle1155(w, z), swizzle1155(z, y), swizzle2266(z, y));
        const F4 fac3 = fac(swizzle3377(w, z), swizzle0044(w, z), swizzle0044(z, y), swizzle3377(z, y));
        const F4 fac4 = fac(swizzle2266(w, z), swizzle0044(w, z), swizzle0044(z, y), swizzle2266(z, y));
        const F4 fac5 = fac(swizzle1155(w, z), swizzle0044(w, z), swizzle0044(z, y), swizzle1155(z, y));

        const F4 SIGN_A = {-1.0f, 1.0f, -1.0f, 1.0f};
        const F4 SIGN_B = {1.0f, -1.0f, 1.0f, -1.0f};

        const F4 temp0 = swizzle0044(y, x);
        const F4 vec0 = swizzle0266(temp0, temp0);
        const F4 temp1 = swizzle1155(y, x);
        const F4 vec1 = swizzle0266(temp1, temp1);
        const F4 temp2 = swizzle2266(y, x);
        const F4 vec2 = swizzle0266(temp2, temp2);
        const F4 temp3 = swizzle3377(y, x);
        const F4 vec3 = swizzle0266(temp3, temp3);

        const F4 inv0 = vmulq_f32(
            SIGN_B, vaddq_f32(vsubq_f32(vmulq_f32(vec1, fac0), vmulq_f32(vec2, fac1)), vmulq_f32(vec3, fac2)));
        const F4 inv1 = vmulq_f32(
            SIGN_A, vaddq_f32(vsubq_f32(vmulq_f32(vec0, fac0), vmulq_f32(vec2, fac3)), vmulq_f32(vec3, fac4)));
        const F4 inv2 = vmulq_f32(
            SIGN_B, vaddq_f32(vsubq_f32(vmulq_f32(vec0, fac1), vmulq_f32(vec1, fac3)), vmulq_f32(vec3, fac5)));
        const F4 inv3 = vmulq_f32(
            SIGN_A, vaddq_f32(vsubq_f32(vmulq_f32(vec0, fac2), vmulq_f32(vec1, fac4)), vmulq_f32(vec2, fac5)));

        const F4 row0 = swizzle0044(inv0, inv1);
        const F4 row1 = swizzle0044(inv2, inv3);
        const F4 row2 = swizzle0246(row0, row1);

        const float dot0 = dot4(x, row2);
        const float rcp0 = 1.0f / dot0;
        return Mat4{Vec4::from_f4(vmulq_n_f32(inv0, rcp0)), Vec4::from_f4(vmulq_n_f32(inv1, rcp0)),
                    Vec4::from_f4(vmulq_n_f32(inv2, rcp0)), Vec4::from_f4(vmulq_n_f32(inv3, rcp0))};
    }

    /// `Mat4::mul_vec4`: ((x*v.x + y*v.y) + z*v.z) + w*v.w per lane.
    Vec4 mul_vec4(Vec4 rhs) const {
        Vec4 res = x_axis * Vec4{rhs.x, rhs.x, rhs.x, rhs.x};
        res = res + y_axis * Vec4{rhs.y, rhs.y, rhs.y, rhs.y};
        res = res + z_axis * Vec4{rhs.z, rhs.z, rhs.z, rhs.z};
        res = res + w_axis * Vec4{rhs.w, rhs.w, rhs.w, rhs.w};
        return res;
    }
    Mat4 mul_mat4(const Mat4& rhs) const {
        return from_cols(mul_vec4(rhs.x_axis), mul_vec4(rhs.y_axis), mul_vec4(rhs.z_axis),
                         mul_vec4(rhs.w_axis));
    }
    friend Mat4 operator*(const Mat4& a, const Mat4& b) { return a.mul_mat4(b); }
    friend Vec4 operator*(const Mat4& a, Vec4 b) { return a.mul_vec4(b); }
};
inline constexpr Mat4 Mat4::IDENTITY =
    Mat4{Vec4{1.0f, 0.0f, 0.0f, 0.0f}, Vec4{0.0f, 1.0f, 0.0f, 0.0f}, Vec4{0.0f, 0.0f, 1.0f, 0.0f},
         Vec4{0.0f, 0.0f, 0.0f, 1.0f}};
S3_DERIVE_EQ(Mat4, x_axis, y_axis, z_axis, w_axis)

}  // namespace s3::game::glam
