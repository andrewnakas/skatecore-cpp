// Port of skate-core/src/player/wipeout_state/contact_response/material11.rs (whole file).
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/player/wipeout_state/contact_response.hpp"

namespace s3::player::wipeout_state::contact_response::material11 {

using namespace s3::player::wipeout_state::math;

void Response::update(SkeletonBody& body, const ContactResponseInput& input) {
    velocity = input.com_velocity_608;
    if (dot(velocity, velocity) > 0.25f) {
        if (input.material11_normal) {
            frames_since_contact = 0;
            contact_latched = true;
            normal = *input.material11_normal;
            if (!active) {
                previous_velocity = reject(previous_velocity, normal);
            }
        }
    }
    active = contact_latched || frames_since_contact < 3;
    if (active) {
        apply_response(body);
        apply_control(body, input.effective_axis_224, input.wipeout_control_2824_2828);
        active_frames = static_cast<std::int32_t>(static_cast<std::uint32_t>(active_frames) + 1u);
    } else {
        previous_velocity = velocity;
        active_frames = 0;
    }
    // Native signed comparison intentionally alternates100->101->100.
    frames_since_contact = frames_since_contact <= 100
                               ? static_cast<std::int32_t>(static_cast<std::uint32_t>(frames_since_contact) + 1u)
                               : 100;
}

void Response::apply_response(SkeletonBody& body) const {
    V retained_delta{};
    if (active_frames < 3) {
        const V difference = reject(sub(previous_velocity, velocity), normal);
        if (dot(difference, velocity) > 0.0f) {
            retained_delta = scale(difference, rs::from_bits(0x3f666666u));
        }
    }
    const V friction = reject(scale(velocity, rs::from_bits(0xbb03126fu)), normal);
    V slope_delta{};
    // 91F9C bge skips the slope term at/above original normal.y limit0.97.
    if (normal[1] < rs::from_bits(0x3f7851ecu)) {
        const V across = cross(V{0.0f, 1.0f, 0.0f, 0.0f}, normal);
        V direction = normalize_or(cross(across, normal), V{});
        if (direction[1] > 0.0f) {
            direction = scale(direction, -1.0f);
        }
        // 92058 loads retained NORMAL; only its Y is zeroed before length.
        V horizontal_normal = normal;
        horizontal_normal[1] = 0.0f;
        const V candidate = scale(direction, length(horizontal_normal) * rs::from_bits(0x3dcccccdu));
        if (dot(candidate, velocity) > 0.0f) {
            slope_delta = candidate;
        }
    }
    apply_velocity_delta(body, add(add(friction, retained_delta), slope_delta));
}

void Response::apply_control(SkeletonBody& body, V effective_axis, std::array<float, 2> control) const {
    const V planar_velocity = reject(velocity, normal);
    const V planar_direction = normalize_or(planar_velocity, V{});
    if (rs::from_bits(0x3f7fbe77u) > dot(V{0.0f, 1.0f, 0.0f, 0.0f}, effective_axis)) {
        const float fraction =
            reciprocal(10.0f) * physics::native_arithmetic::vector_min(10.0f, length(planar_velocity));
        const V in{control[0], 0.0f, control[1], 0.0f};
        const V tangent = reject(in, normal);
        const V lateral = reject(tangent, planar_direction);
        apply_velocity_delta(body, scale(lateral, fraction * rs::from_bits(0x3d4ccccdu)));
    }
}

}  // namespace s3::player::wipeout_state::contact_response::material11
