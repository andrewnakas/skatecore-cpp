// Port of skate-core/src/player/wipeout_state/body.rs (whole file).
#include "s3/core/player/wipeout_state/body.hpp"

namespace s3::player::wipeout_state::body {

using s3::math::Vector3;

void set_velocity(SkeletonBody& body, V velocity) {
    auto& bodies = body.bodies_mut();
    for (std::size_t i = 1; i < 24; ++i) {
        bodies[i].rates.linear_velocity = Vector3::new_(velocity[0], velocity[1], velocity[2]);
    }
}
void add_velocity(SkeletonBody& body, V velocity) {
    auto& bodies = body.bodies_mut();
    for (std::size_t i = 1; i < 24; ++i) {
        bodies[i].rates.linear_velocity.x += velocity[0];
        bodies[i].rates.linear_velocity.y += velocity[1];
        bodies[i].rates.linear_velocity.z += velocity[2];
    }
}
void remove_normal_velocity(SkeletonBody& body, V normal) {
    for (std::size_t i = 1; i < 24; ++i) {
        const Vector3 v3 = body.bodies()[i].rates.linear_velocity;
        const V v{v3.x, v3.y, v3.z, 0.0f};
        const V projected = math::scale(normal, math::dot(normal, v));
        const V next = math::madd(projected, i < 15 ? 0.1f : 0.0f, math::sub(v, projected));
        body.bodies_mut()[i].rates.linear_velocity = Vector3::new_(next[0], next[1], next[2]);
    }
}
void blend_velocity(SkeletonBody& body, V target) {
    auto& bodies = body.bodies_mut();
    for (std::size_t i = 1; i < 24; ++i) {
        const Vector3 v = bodies[i].rates.linear_velocity;
        const V average = math::madd(target, 0.5f, V{v.x * 0.5f, v.y * 0.5f, v.z * 0.5f, 0.0f});
        const V delta = math::sub(average, target);
        const float length = math::length(delta);
        const V next = length <= 2.0f ? average : math::madd(delta, 2.0f / length, target);
        bodies[i].rates.linear_velocity = Vector3::new_(next[0], next[1], next[2]);
    }
}
void limit_velocity(SkeletonBody& body, float maximum) {
    auto& bodies = body.bodies_mut();
    for (std::size_t i = 1; i < 24; ++i) {
        const Vector3 v3 = bodies[i].rates.linear_velocity;
        const V v{v3.x, v3.y, v3.z, 0.0f};
        if (math::dot(v, v) > maximum * maximum) {
            const V next = math::scale(v, math::reciprocal(math::length(v)) * maximum);
            bodies[i].rates.linear_velocity = Vector3::new_(next[0], next[1], next[2]);
        }
    }
}
void set_drag(SkeletonBody& body, float linear, float angular) {
    for (auto& part : body.bodies_mut()) {
        part.inertia.linear_drag = linear * rs::from_bits(0x426FFFFFu);
        part.inertia.angular_drag = angular * rs::from_bits(0x426FFFFFu);
    }
}
void set_wipeout_drag(SkeletonBody& body, float drag) {
    set_drag(body, math::clamp(drag, 0.0f, 1.0f), math::clamp((drag + 1.0f) * 0.5f, 0.0f, 1.0f));
}
void special_surface(SkeletonBody& body, float height) {
    static constexpr std::array<float, 23> WEIGHTS = {1.7f, 1.5f, 1.2f, 1.0f, 1.0f, 1.0f, 1.2f, 1.0f,
                                                      1.0f, 1.0f, 1.2f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f,
                                                      0.5f, 0.6f, 0.5f, 0.5f, 0.5f, 0.6f, 1.0f};
    for (std::size_t i = 1; i < 24; ++i) {
        const float depth = (height + 0.1f) - body.record.pose[i][3][1];
        float drag;
        if (depth > 0.0f) {
            body.apply_part_displacement(i, {0.0f, rs::mul_add(depth, 0.6f, 0.1f) * WEIGHTS[i - 1], 0.0f, 0.0f});
            drag = 0.1f;
        } else {
            drag = 0.005f;
        }
        body.bodies_mut()[i].inertia.angular_drag = drag * rs::from_bits(0x426FFFFFu);
        body.bodies_mut()[i].inertia.linear_drag = drag * rs::from_bits(0x426FFFFFu);
    }
}

}  // namespace s3::player::wipeout_state::body
