// Port of skate-core/src/camera/orientation_math.rs.
#include "s3/core/camera/orientation_math.hpp"

#include "s3/core/trigonometry.hpp"

namespace s3::camera::orientation_math {
namespace {

std::array<float, 3> cross(std::array<float, 3> a, std::array<float, 3> b) {
    return {
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
    };
}

}  // namespace

F4 quaternion_from_angles(float pitch, float yaw, float roll) {
    const auto [sp, cp] = trigonometry::sin_cos(pitch * 0.5f);
    const auto [sy, cy] = trigonometry::sin_cos(yaw * 0.5f);
    const auto [sr, cr] = trigonometry::sin_cos(roll * 0.5f);
    const float cp_cr = cp * cr;
    const float sp_cr = sp * cr;
    const float sp_sr = sp * sr;
    const float cp_sr = cp * sr;
    return {
        cy * sp_cr - sy * cp_sr,
        rs::mul_add(cy, sp_sr, sy * cp_cr),
        cy * cp_sr - sy * sp_cr,
        rs::mul_add(cy, cp_cr, sy * sp_sr),
    };
}

Basis3 basis_from_angles(float pitch, float yaw, float roll) {
    const auto [sp, cp] = trigonometry::sin_cos(pitch);
    const auto [sy, cy] = trigonometry::sin_cos(yaw);
    const auto [sr, cr] = trigonometry::sin_cos(roll);
    const float cp_sr = cp * sr;
    const float cp_cr = cp * cr;
    const float sp_sr = sp * sr;
    const float sp_cr = sp * cr;
    Basis3 out;
    out.columns = {{
        {cy * cr, cy * sr, -sy},
        {sy * sp_cr - cp_sr, rs::mul_add(sy, sp_sr, cp_cr), cy * sp},
        {rs::mul_add(sy, cp_cr, sp_sr), sy * cp_sr - sp_cr, cy * cp},
    }};
    return out;
}

Basis3 rotate_about_axis(const Basis3& basis, std::array<float, 3> axis, float angle) {
    const float cosine = trigonometry::cos(angle);
    const float sine = trigonometry::sin(angle);
    const float x = axis[0];
    const float y = axis[1];
    const float z = axis[2];
    const float sx = sine * x;
    const float sy = sine * y;
    const float sz = sine * z;
    const float one_minus_cosine = 1.0f - cosine;
    const float tx = one_minus_cosine * x;
    const float ty = one_minus_cosine * y;
    const float tz = one_minus_cosine * z;
    const std::array<std::array<float, 3>, 3> rotation = {{
        {rs::mul_add(x, tx, cosine), rs::mul_add(tx, y, sz), tx * z - sy},
        {ty * x - sz, rs::mul_add(ty, y, cosine), rs::mul_add(ty, z, sx)},
        {rs::mul_add(tz, x, sy), tz * y - sx, rs::mul_add(z, tz, cosine)},
    }};
    Basis3 out;
    out.columns = rs::map(basis.columns, [&](const std::array<float, 3>& column) {
        return rs::from_fn<3>([&](std::size_t i) {
            return rs::mul_add(rotation[2][i], column[2],
                               rs::mul_add(rotation[1][i], column[1], rotation[0][i] * column[0]));
        });
    });
    return out;
}

Basis3 look_basis(std::array<float, 3> at) {
    const auto right = normalize(cross({0.0f, 1.0f, 0.0f}, at));
    const auto up = normalize(cross(at, right));
    Basis3 out;
    out.columns = {right, up, at};
    return out;
}

}  // namespace s3::camera::orientation_math
