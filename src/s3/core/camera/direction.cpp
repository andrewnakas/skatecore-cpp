// Port of skate-core/src/camera/direction.rs.
#include "s3/core/camera/direction.hpp"

#include <cstdint>

#include "s3/core/camera/angle_tracker.hpp"
#include "s3/core/camera/vector_tracker.hpp"

namespace s3::camera {
namespace {

float clamp(float value) {
    const float lower = -1.0f > value ? -1.0f : value;
    return 1.0f < lower ? 1.0f : lower;
}

// Native inline asin polynomial: 822F9820/30/40, radicand822FB840.
// Keep it before the acos subtraction used by the separate82453298 operation.
float asin_lane(float value) {
    const float a = rs::abs(value);
    const float cube = (value * value) * a;
    auto c = [](std::uint32_t bits) { return rs::from_bits(bits); };
    float p0 = rs::mul_add(c(0x400b1889), a, c(0xc0d1360e));
    float p1 = rs::mul_add(c(0x3e663246), a, c(0xbf983f2f));
    float p2 = rs::mul_add(c(0xbed65553), a, c(0x408980bd));
    float p3 = rs::mul_add(c(0xbd6dd42d), a, c(0x3f1dd7b6));
    p0 = rs::mul_add(p0, a, c(0x40af6ad8));
    p1 = rs::mul_add(p1, a, c(0x3fb58485));
    p2 = rs::mul_add(p2, a, c(0xc08f6ad9));
    p3 = rs::mul_add(p3, a, c(0xbfaf4418));
    const float left = rs::mul_add(p1, cube, p0);
    const float right = rs::mul_add(p3, cube, p2);
    const float radicand = c(0x3f800001) - a;
    float r = physics::reciprocal_sqrt::estimate(radicand);
    const float correction = rs::mul_add(-(radicand * 0.5f), r * r, 0.5f);
    r = rs::mul_add(r, correction, r);
    const float residual = rs::mul_add(-a, value, value) * right;
    return rs::mul_add(residual, r, value * left);
}

}  // namespace

F4 direction_to_angles(F4 direction) {
    const F4 horizontal = {direction[0], 0.0f, direction[2], 0.0f};
    const float magnitude = vector_tracker::length(horizontal);
    if (!(magnitude > rs::from_bits(0x38d1b717))) {
        return F4{};
    }
    const float inverse = vector_tracker::refined_reciprocal(magnitude);
    const float x = inverse * horizontal[0];
    const float z = inverse * horizontal[2];
    float component;
    float base;
    if (x >= 0.0f) {
        if (z >= 0.0f) {
            component = x;
            base = 0.0f;
        } else {
            component = -z;
            base = rs::from_bits(0x3fc90fdb);
        }
    } else if (z <= 0.0f) {
        component = -x;
        base = rs::from_bits(0x40490fdb);
    } else {
        component = z;
        base = rs::from_bits(0x4096cbe4);
    }
    const float heading = normalize_angle(asin_lane(clamp(component)) + base);
    return {asin_lane(clamp(direction[1])), heading, 0.0f, 0.0f};
}

}  // namespace s3::camera
