// Port of skate-core/src/player/offboard/slide.rs (whole file).
#include "s3/core/player/offboard/slide.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/player/offboard/contact_correction.hpp"

namespace s3::player::offboard::slide {

using physics::native_arithmetic::dot3;

namespace {
float select(float test, float positive, float negative) { return test >= 0.0f ? positive : negative; }
Vector safe_unit(Vector vector) {
    const float square = dot3(vector, vector);
    const float inverse = contact_correction::inverse_length(square);
    if (contact_correction::magnitude(square) > rs::from_bits(0x358637bdu)) {
        return rs::map(vector, [&](float v) { return v * inverse; });
    }
    return Vector{};
}
/// 82C1E170: project against safe unit, subtract the original supplied direction.
Vector reject_positive(Vector vector, Vector direction) {
    const float projection = dot3(vector, safe_unit(direction));
    if (projection > 0.0f) {
        return rs::from_fn<4>([&](std::size_t i) { return vector[i] - direction[i] * projection; });
    }
    return vector;
}
}  // namespace

void Sliding::update(const Input& input, const point_graph::PointGraph<8>& slide_vs_slope,
                     const point_graph::PointGraph<8>& slide_vs_speed) {
    // 82D7CC44 returns without changing either retained field in this mode.
    if (input.special_mode_714) return;
    const Vector down{0.0f, -1.0f, 0.0f, 0.0f};
    const Vector normal = input.surface_normal_560;
    const float projection = dot3(normal, down);
    const Vector downhill = rs::from_fn<4>([&](std::size_t i) { return down[i] - normal[i] * projection; });
    const float tilt = -downhill[1];
    const float lower = select(-tilt, 0.0f, tilt);
    const float slope = select(1.0f - lower, lower, 1.0f);
    const Vector uphill = rs::map(safe_unit(downhill), [](float v) { return rs::from_bits(rs::to_bits(v) ^ 0x80000000u); });
    const float speed = dot3(input.movement_velocity_480, uphill);
    const float factor = -(slide_vs_slope.evaluate(slope) * slide_vs_speed.evaluate(speed));
    // 82F82588 initializes830BD400 by splatting822F8B40 (-9.8).
    const Vector acceleration = contact_correction::clamp_length(
        rs::map(downhill, [&](float v) { return v * (rs::from_bits(0xc11ccccdu) * factor); }), 5.0f);
    velocity_528 = rs::from_fn<4>([&](std::size_t i) {
        return rs::mul_add(acceleration[i], rs::from_bits(0x3d4ccccdu), velocity_528[i] * 0.95f);
    });
    const float threshold = rs::from_bits(0x3c23d70bu);
    if (threshold > dot3(acceleration, acceleration)) {
        velocity_528 = rs::map(velocity_528, [](float v) { return v * 0.9f; });
    }
    if (input.contact_direction_400) {
        velocity_528 = reject_positive(velocity_528, rs::map(*input.contact_direction_400, [](float v) { return -v; }));
    }
    active_710 = dot3(velocity_528, velocity_528) > threshold;
}

}  // namespace s3::player::offboard::slide
