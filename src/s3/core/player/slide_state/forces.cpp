// Port of skate-core/src/player/slide_state/forces.rs (whole file).
#include "s3/core/player/slide_state/forces.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::slide_state::forces {

using physics::board_motion_output::inverse_length_squared;
using physics::force_queue::QueuedPointForce;
using s3::math::Vector3;

namespace {
float dot(V a, V b) { return physics::native_arithmetic::dot3(a, b); }
V scale(V v, float s) { return rs::map(v, [&](float x) { return x * s; }); }
V madd(V v, float s, V b) {
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(v[i], s, b[i]); });
}
V reject(V v, V n) {
    const float d = dot(v, n);
    return rs::from_fn<4>([&](std::size_t i) { return v[i] - n[i] * d; });
}
V cross(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        rs::mul_add(-a[3], b[3], a[3] * b[3]),
    };
}
V normalize(V v) { return scale(v, inverse_length_squared(dot(v, v), 2)); }
V normalize_safe(V v) {
    const float d = dot(v, v);
    const float inv = inverse_length_squared(d, 2);
    const float length = d == 0.0f ? 0.0f : d * inv;
    if (length > rs::from_bits(0x358637bdu)) {
        return scale(v, inv);
    }
    return V{};
}
float fsel(float v, float a, float b) { return v >= -0.0f ? a : b; }
Vector3 xyz(V v) { return Vector3::new_(v[0], v[1], v[2]); }
}  // namespace

V angular_correction(const SlideSettings& s, const SlideSurface& surface, SlideInput p) {
    const float remap = s.input_remap.evaluate(rs::abs(p.slide));
    float age = p.elapsed - 0.5f;
    age = fsel(-age, 0.0f, age);
    age = fsel(1.0f - age, age, 1.0f);
    const float weight = s.remap_vs_speed.evaluate(p.surface_speed) * age;
    const float pi = rs::from_bits(0x40490fdbu);
    const float signed_ = fsel(p.slide, remap, -remap);
    const float angle = rs::mul_add(p.slide * pi, 1.0f - weight, (signed_ * pi) * weight);
    const V velocity = normalize(p.velocity);
    const V side = normalize(cross(p.normal, velocity));
    const V desired = madd(velocity, trigonometry::cos(angle), scale(side, trigonometry::sin(angle)));
    V error = cross(p.reference_forward, desired);
    if (dot(p.reference_forward, desired) < 0.0f) {
        error = error[1] < 0.0f ? rs::map(p.normal, [](float v) { return -v; }) : p.normal;
    }
    const float strength =
        rs::mul_add(1.0f - s.softest_wheel_spin, p.wheel_hardness, s.softest_wheel_spin) * surface.yaw_strength;
    const V displacement =
        scale(madd(error, strength, scale(p.angular_velocity, surface.yaw_damping)), s.angular_force);
    return scale(p.normal, dot(displacement, p.normal));
}

QueuedPointForce sliding_force(const SlideSettings& s, const SlideSurface& surface, SlideInput p) {
    const V tangent = reject(p.velocity, p.normal);
    const V lateral = scale(p.side, dot(tangent, p.side));
    const float response = surface.speed_to_force.evaluate(p.absolute_speed);
    // Original ble is !GT, so unordered follows the <= branch too.
    const float hardness = response > 0.0f ? p.wheel_hardness : 1.0f;
    const float strength = rs::mul_add(1.0f - s.softest_wheel_force, hardness, s.softest_wheel_force) * response;
    const V side = normalize_safe(cross(p.normal, p.velocity));
    const V longitudinal = reject(scale(lateral, strength), normalize_safe(side));
    float angle = 0.0f;
    if (dot(p.velocity, p.velocity) * dot(p.effective_forward, p.effective_forward) > rs::from_bits(0x37800000u)) {
        angle = riding::collision_response::signed_angle(xyz(reject(p.velocity, p.normal)),
                                                         xyz(reject(p.effective_forward, p.normal)), xyz(p.normal));
    }
    const float turns = angle * rs::from_bits(0x3e22f983u);
    const float fraction = turns - rs::floor(turns);
    const float wrapped = (fraction - (fraction > 0.5f ? 1.0f : 0.0f)) * rs::from_bits(0x40c90fdbu);
    const float degrees = wrapped * rs::from_bits(0x42652ee1u);
    const float sign = degrees > 0.0f ? 1.0f : -1.0f;
    const V angular = scale(scale(side, s.force_vs_angle.evaluate(rs::abs(degrees))),
                            s.force_vs_speed.evaluate(p.surface_speed));
    const V force = madd(angular, sign, longitudinal);
    // bge skips negation for unordered as well as nonnegative.
    const float y = dot(force, p.velocity) < 0.0f ? -s.force_y_offset : s.force_y_offset;
    return QueuedPointForce{9, xyz(force), Vector3::new_(0.0f, y, 0.0f)};
}

}  // namespace s3::player::slide_state::forces
