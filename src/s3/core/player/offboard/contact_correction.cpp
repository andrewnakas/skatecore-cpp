// Port of skate-core/src/player/offboard/contact_correction.rs (whole file).
#include "s3/core/player/offboard/contact_correction.hpp"

namespace s3::player::offboard::contact_correction {

namespace {
float dot(Vector a, Vector b) { return physics::native_arithmetic::dot3(a, b); }
float select(float test, float positive, float negative) { return test >= 0.0f ? positive : negative; }
Vector reject(Vector vector, Vector axis) {
    const float projection = dot(axis, vector);
    return rs::from_fn<4>([&](std::size_t i) { return vector[i] - axis[i] * projection; });
}
}  // namespace

void ContactCorrection::update(const Input& input) {
    displacement = Vector{};
    const Vector a = input.collision_displacements[0];
    const Vector b = input.collision_displacements[1];
    const float square_a = dot(a, a);
    const float square_b = dot(b, b);
    // The second vector wins equality and unordered comparison.
    const Vector selected = square_a > square_b ? a : b;
    const float maximum = select(square_a - square_b, square_a, square_b);
    active = false;
    // 82D7CA7C ble skips the branch only for ordered <=.
    if (!(maximum <= rs::from_bits(0x38d1b717u))) {
        const float inverse = inverse_length(dot(selected, selected));
        direction = rs::map(selected, [&](float value) { return value * inverse; });
        active = true;
        const float length = magnitude(maximum);
        float excess = length - rs::from_bits(0x3d4ccccdu);
        excess = select(excess, excess, 0.0f);
        const Vector disp = rs::map(direction, [&](float value) { return (value * excess) * 0.5f; });
        displacement = clamp_length(reject(disp, input.projection_axis_416), rs::from_bits(0x3dcccccdu));
    }
    displacement = reject(displacement, input.up_axis_16);
}

}  // namespace s3::player::offboard::contact_correction
