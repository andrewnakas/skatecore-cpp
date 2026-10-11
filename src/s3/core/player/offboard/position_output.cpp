// Port of skate-core/src/player/offboard/position_output.rs (whole file).
#include "s3/core/player/offboard/position_output.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/physics/reciprocal_sqrt.hpp"
#include "s3/core/physics/skeleton_root.hpp"

namespace s3::player::offboard::position_output {

using physics::native_arithmetic::dot3;

namespace {
const float DT = rs::from_bits(0x3c888889u);
Vector subtract(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2], a[3] - b[3]}; }
float select(float test, float positive, float negative) { return test >= 0.0f ? positive : negative; }
Vector cross(Vector a, Vector b) {
    return Vector{rs::mul_add(-a[2], b[1], a[1] * b[2]), rs::mul_add(-a[0], b[2], a[2] * b[0]),
                  rs::mul_add(-a[1], b[0], a[0] * b[1]), rs::mul_add(-a[3], b[3], a[3] * b[3])};
}
Vector normalize(Vector vector) {
    const float squared = dot3(vector, vector);
    float inverse = physics::reciprocal_sqrt::estimate(squared);
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-squared, inverse * inverse, 1.0f), inverse);
    }
    return rs::map(vector, [&](float value) { return value * inverse; });
}
}  // namespace

void FrameOutput::update(Vector forward_32, Vector frame_up_448, Vector projection_axis_416, Vector position_48) {
    const Vector right = normalize(cross(frame_up_448, forward_32));
    const Vector forward = normalize(cross(right, projection_axis_416));
    frame = physics::skeleton_root::orthonormalize(Matrix{right, frame_up_448, forward, frame[3]});
    Vector change = rs::from_fn<4>([&](std::size_t i) { return (position_48[i] - frame[3][i]) * 60.0f - velocity[i]; });
    change[1] = select(change[1] - -1.0f, change[1], -1.0f);
    velocity = rs::from_fn<4>([&](std::size_t i) { return velocity[i] + change[i]; });
    frame[3] = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(velocity[i], DT, frame[3][i]); });
}

void update_position(Vector& position_368, const PositionInput& input) {
    const Vector up = input.projection_axis_416;
    Vector origin = input.previous_origin_112;
    if (input.use_override_711) {
        origin = input.override_origin_592;
    } else if ((input.contact_flags_176 & 1u) != 0) {
        if (dot3(subtract(input.contact_origin_0, origin), up) > 0.0f) origin = input.contact_origin_0;
    }
    if ((input.contact_flags_176 & 2u) != 0) {
        const float contact_height = dot3(subtract(input.contact_origin_96, origin), up);
        const float frame_height = dot3(subtract(input.frame_position_176, origin), up);
        float height = select(contact_height - frame_height, contact_height, frame_height);
        height = select(height - 0.3f, 0.3f, height);
        // 82D80638 ble: unordered takes the update branch.
        if (!(height <= 0.0f)) {
            origin = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(up[i], height, origin[i]); });
        }
    }
    Vector delta = subtract(input.animation_position_272, origin);
    const float projection = dot3(up, delta);
    const Vector planar = rs::from_fn<4>([&](std::size_t i) { return delta[i] - up[i] * projection; });
    const Vector target = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(up[i], 0.95f, origin[i] + planar[i]); });
    delta = subtract(target, position_368);
    const float height = dot3(delta, up);
    const float lower = select(-0.1f - height, -0.1f, height);
    const float bounded = select(0.1f - lower, lower, 0.1f);
    const float correction = bounded - height;
    position_368 = rs::from_fn<4>([&](std::size_t i) { return position_368[i] + rs::mul_add(up[i], correction, delta[i]); });
}

}  // namespace s3::player::offboard::position_output
