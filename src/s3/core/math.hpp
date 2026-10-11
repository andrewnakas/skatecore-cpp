// Port of skate-core/src/math.rs: shared scalar types. Rendering conversions stay outside the core.
#pragma once
#include <array>
#include <ostream>

#include "s3/core/rs.hpp"

namespace s3::math {

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    static constexpr Vector3 new_(float x, float y, float z) { return Vector3{x, y, z}; }
    static const Vector3 ZERO;
};
inline constexpr Vector3 Vector3::ZERO = Vector3{0.0f, 0.0f, 0.0f};
S3_DERIVE_EQ(Vector3, x, y, z)
inline std::ostream& operator<<(std::ostream& os, const Vector3& v) {
    return os << "Vector3 { x: " << v.x << ", y: " << v.y << ", z: " << v.z << " }";
}

struct Basis3 {
    std::array<std::array<float, 3>, 3> columns{};
};
S3_DERIVE_EQ(Basis3, columns)
inline std::ostream& operator<<(std::ostream& os, const Basis3& b) {
    os << "Basis3 [";
    for (const auto& c : b.columns) os << " [" << c[0] << ", " << c[1] << ", " << c[2] << "]";
    return os << " ]";
}

}  // namespace s3::math
