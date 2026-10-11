// Port of skate-core/src/physics/board_dynamic_normal.rs.
#include "s3/core/physics/board_dynamic_normal.hpp"

namespace s3::physics::board_dynamic_normal {

using board_motion_output::add;
using board_motion_output::dot;
using board_motion_output::inverse_length_squared;
using board_motion_output::scale;
using board_motion_output::subtract;

namespace {
constexpr Vector3 UP = Vector3{0.0f, 1.0f, 0.0f};
constexpr Vector3 ZERO = Vector3{0.0f, 0.0f, 0.0f};
}  // namespace

BoardDynamicNormal BoardDynamicNormal::new_() { return BoardDynamicNormal{UP, UP, ZERO, ZERO}; }

void BoardDynamicNormal::update(const BoardGroundState& contacts, Vector3 gravity, float previous_ground_speed,
                                const DynamicNormalSettings& settings) {
    // 82C02344 runs Body::UpdatePostPhysics before82C0234C calls this
    // filter.82C02444/60/7C read the existing Body432/448/464 array.
    // Paired S2:82B37E5C/64 calls Body::UpdatePostPhysics then
    // UpdateUpVector82B26158, which reads CollisionInfo acceleration.
    // No live body, timestep, duplicate history or mirror belongs here.
    if (contacts.wheel_contact_count == 0) {
        acceleration = ZERO;
        return;
    }
    Vector3 sum = ZERO;
    // The original body tests780,781,782, then normalizes. There is no
    // fourth-wheel accumulation in this function; retain that distinction.
    for (std::size_t i = 0; i < 3; ++i) {
        if (contacts.parts[i].in_contact) {
            sum = subtract(add(sum, contacts.accelerations[i]), gravity);
        }
    }
    const Vector3 target = normalize(sum);
    delta = scale(delta, settings.speed_damping);
    const Vector3 error = scale(subtract(target, normal), settings.up_vector_damping);
    const auto [direction, magnitude] = normalize_length(error);
    const float maximum =
        settings.maximum_delta_vs_speed.evaluate(rs::abs(previous_ground_speed) / settings.speed_scale) *
        settings.maximum_delta;
    const float nonnegative = -magnitude >= -0.0f ? 0.0f : magnitude;
    const float amount = maximum - nonnegative >= -0.0f ? nonnegative : maximum;
    delta = Vector3{
        rs::mul_add(direction.x, amount, delta.x),
        rs::mul_add(direction.y, amount, delta.y),
        rs::mul_add(direction.z, amount, delta.z),
    };
    normal = normalize(add(normal, delta));
    last_contact_normal = contacts.overall_normal;
}

Vector3 normalize(Vector3 v) { return normalize_length(v).first; }

std::pair<Vector3, float> normalize_length(Vector3 v) {
    const float squared = dot(v, v);
    const float inverse = inverse_length_squared(squared, 2);
    const float length = squared == 0.0f ? 0.0f : squared * inverse;
    // 830BD350's verified initializer is float1e-6. Source compares length.
    const Vector3 normal = length > rs::from_bits(0x358637bdu) ? scale(v, inverse) : ZERO;
    return {normal, length};
}

}  // namespace s3::physics::board_dynamic_normal
