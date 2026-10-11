// Port of skate-core/src/player/offboard/ground_input.rs and ground_input/math.rs.
#include "s3/core/player/offboard/ground_input.hpp"

#include "s3/core/player/offboard/ground_input/math.hpp"

namespace s3::player::offboard::ground_input {

namespace math {
namespace {
const float HALF_PI = rs::from_bits(0x3fc90fdbu);
const float PI = rs::from_bits(0x40490fdbu);
float inverse_sqrt(float q) {
    float r = 1.0f / rs::sqrt(q);
    for (int i = 0; i < 2; ++i) {
        const float square = r * r;
        const float half = r * 0.5f;
        const float error = rs::mul_add(-q, square, 1.0f);
        r = rs::mul_add(half, error, r);
    }
    return r;
}
float reciprocal_two(float d) {
    const float seed = 1.0f / d;
    const float first = rs::mul_add(seed, rs::mul_add(-d, seed, 1.0f), seed);
    const float second = rs::mul_add(first, rs::mul_add(-d, first, 1.0f), first);
    // Original compares the FIRST refinement before selecting the second.
    return rs::is_nan(first) ? seed : second;
}
}  // namespace

Vector normalize(Vector v) {
    const float r = inverse_sqrt(dot(v, v));
    return Vector{v[0] * r, v[1] * r, v[2] * r};
}
float length(Vector v) {
    const float q = dot(v, v);
    const float result = q * inverse_sqrt(q);
    return q == 0.0f ? 0.0f : result;
}
Vector inverse_frame(Vector right, Vector up, Vector forward, Vector v) {
    const Vector c0 = cross(up, forward);
    const Vector c1 = cross(forward, right);
    const Vector c2 = cross(right, up);
    const float determinant = dot(right, c0);
    float reciprocal = 1.0f / determinant;
    for (int i = 0; i < 2; ++i) {
        reciprocal = rs::mul_add(reciprocal, rs::mul_add(-determinant, reciprocal, 1.0f), reciprocal);
    }
    // Scale inverse columns before multiplying and accumulating input components.
    const Vector c[3] = {c0, c1, c2};
    Vector out{};
    for (std::size_t i = 0; i < 3; ++i) {
        const float x = c[i][0] * reciprocal;
        const float y = c[i][1] * reciprocal;
        const float z = c[i][2] * reciprocal;
        out[i] = rs::mul_add(z, v[2], rs::mul_add(y, v[1], x * v[0]));
    }
    return out;
}
float signed_angle(float x, float y) {
    const float seed = 1.0f / y;
    const float reciprocal = rs::mul_add(seed, rs::mul_add(-y, seed, 1.0f), seed);
    float angle = rational_atan(rs::mul_add(x, reciprocal, 0.0f));
    if (y < 0.0f) angle += rs::copysign(PI, x);
    if (y == 0.0f) angle = rs::copysign(HALF_PI, x);
    return angle;
}
float rational_atan(float original) {
    const float absolute = rs::abs(original);
    const bool invert = absolute > 1.0f;
    const float reciprocal = reciprocal_two(absolute);
    const float x = invert ? reciprocal : absolute;
    const float base_offset = invert ? HALF_PI : 0.0f;
    const float reduced_offset = invert ? rs::from_bits(0x3f860a92u) : rs::from_bits(0x3f060a92u);
    const float denominator = x + rs::from_bits(0x3fddb3d7u);
    const float numerator = rs::mul_add(x, rs::from_bits(0x3f3b67afu), x) - 1.0f;
    const float transformed = numerator * reciprocal_two(denominator);
    const bool reduce = x > rs::from_bits(0x3e8930a3u);
    const float t = reduce ? transformed : x;
    const float offset = reduce ? reduced_offset : base_offset;
    const float q = t * t;
    float p = rs::mul_add(q, rs::from_bits(0xbf566bd7u), rs::from_bits(0xc107e9fbu));
    p = rs::mul_add(q, p, rs::from_bits(0xc1a40bfeu));
    p = rs::mul_add(q, p, rs::from_bits(0xc15b0533u));
    p *= q;
    float d = q + rs::from_bits(0x4170624fu);
    d = rs::mul_add(q, d, rs::from_bits(0x426e5052u));
    d = rs::mul_add(q, d, rs::from_bits(0x42ac5090u));
    d = rs::mul_add(q, d, rs::from_bits(0x422443e6u));
    const float correction = p * reciprocal_two(d);
    const float polynomial = rs::mul_add(t, correction, t);
    float result = rs::abs(t) < rs::from_bits(0x39800000u) ? t : polynomial;
    if (invert) result = -result;
    result += offset;
    if (original < 0.0f) result = -result;
    const float huge = rs::from_bits(0x7e800000u);
    if (original > huge) result = HALF_PI;
    if (original < -huge) result = -HALF_PI;
    return result;
}
}  // namespace math

GroundInputOutput calculate(const GroundInput& input, const point_graph::PointGraph<8>& curve_a,
                            const point_graph::PointGraph<8>& turn_vs_stick_angle) {
    using namespace ground_input::math;
    if ((input.processed_flags_2472 & 0x10000000u) != 0) {
        return GroundInputOutput{input.processed_direct_2684, input.processed_direct_2680, {}};
    }
    const Vector raw{input.processed_stick_2692, 0.0f, input.processed_stick_2688};
    const Vector up{0.0f, 1.0f, 0.0f};
    Vector right{1.0f, 0.0f, 0.0f};
    Vector forward{0.0f, 0.0f, 1.0f};
    // Signed comparison: negative vertical does not receive an identity fallback.
    if (dot(input.frame_forward_112, up) < rs::from_bits(0x3f7fbe77u)) {
        right = normalize(cross(up, input.frame_forward_112));
        forward = normalize(cross(right, up));
    }
    const Vector local = inverse_frame(right, up, forward, raw);
    const float angle = signed_angle(local[0], -local[2]) * rs::from_bits(0x3ea2f983u);
    // Scalar fsel accepts either signed zero; unordered input selects -1.
    const float sign = angle >= 0.0f ? 1.0f : -1.0f;
    const float absolute_angle = angle * sign;
    const float magnitude = length(local);
    return GroundInputOutput{
        (curve_a.evaluate(absolute_angle) * magnitude) * input.processed_scale_2912,
        ((turn_vs_stick_angle.evaluate(absolute_angle) * magnitude) * sign) * input.processed_scale_2908,
        {raw[0], raw[1], raw[2], 0.0f},
    };
}

}  // namespace s3::player::offboard::ground_input
