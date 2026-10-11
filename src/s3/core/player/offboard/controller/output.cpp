// Port of skate-core/src/player/offboard/controller/output.rs: frame helper82D2CDC0.
// (`export` lives in controller.cpp.)
#include "s3/core/player/offboard/controller/output.hpp"

namespace s3::player::offboard::controller {

namespace {
namespace out {
float dot(Vector a, Vector b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
Vector cross(Vector a, Vector b) {
    return Vector{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
float invsqrt(float q) {
    float r = 1.0f / rs::sqrt(q);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-q, r * r, 1.0f), r);
    }
    return r;
}
Vector normalize(Vector v) {
    const float r = invsqrt(dot(v, v));
    return rs::map(v, [&](float x) { return x * r; });
}
}  // namespace out
}  // namespace


static constexpr Vector ZERO_{0.0f, 0.0f, 0.0f, 0.0f};
static constexpr Frame IDENTITY_{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, ZERO_}};

Frame build_frame(Vector up, Vector forward) {
    const Vector right0 = out::cross(up, forward);
    const float q = out::dot(right0, right0);
    const float raw_length = q * out::invsqrt(q);
    const float length = q == 0.0f ? 0.0f : raw_length;
    // 830BD300 initialized82F82690 from821647E0, bits37800000.
    if (!(length > rs::from_bits(0x37800000u))) {
        return IDENTITY_;
    }
    float inverse = 1.0f / length;
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse, rs::mul_add(-inverse, length, 1.0f), inverse);
    }
    const Vector right = rs::map(right0, [&](float x) { return x * inverse; });
    up = out::normalize(up);
    forward = out::normalize(out::cross(right, up));
    return Frame{right, up, forward, ZERO_};
}

}  // namespace s3::player::offboard::controller
