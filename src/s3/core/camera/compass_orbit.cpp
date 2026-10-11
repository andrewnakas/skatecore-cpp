// Port of skate-core/src/camera/compass_orbit.rs.
//! Free-look compass82DF4100. Retains native orbit position, angular dead-zone,
//! thirty-degree limit relative to the last camera, and two-unit arm length.
#include "s3/core/camera/compass.hpp"
#include "s3/core/camera/orientation_math.hpp"
#include "s3/core/camera/vector_tracker.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::camera {
namespace {

using vector_tracker::dot;
using vector_tracker::length;
using vector_tracker::refined_reciprocal;

F4 normalize_safe(F4 v) {
    if (length(v) > rs::from_bits(0x358637bd)) {
        return orientation_math::normalize(v);
    }
    return F4{};
}
F4 add(F4 a, F4 b) { return rs::from_fn<4>([&](std::size_t i) { return a[i] + b[i]; }); }
F4 cross(F4 a, F4 b) {
    return {
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}
F4 rotate(F4 q, F4 v) {
    const F4 first = cross(q, v);
    const F4 intermediate = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(q[3], v[i], first[i]); });
    const F4 second = cross(q, intermediate);
    return rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(2.0f, second[i], v[i]); });
}

}  // namespace

using compass::heading;
using compass::horizontal;
using compass::sub;
using compass::wrap;

void Compass::update_orbit(float dt, const CompassInputs& input) {
    const F4 position = input.transform[3];
    if (input.selected_compass != 6 || dt == 0.0f) {
        this->orbit_position = input.camera_position;
        this->headings[6] = heading(horizontal(sub(position, this->orbit_position)));
        this->orbit_delta = 0.0f;
        return;
    }
    const float stick = -input.look[0];
    float step = 0.0f;
    float retained = 0.95f;
    if (rs::abs(stick) > 0.3f) {
        retained = 0.9f;
        step = ((stick - (stick >= 0.0f ? 0.3f : -0.3f)) * dt) * rs::from_bits(0x408f9d9c);
    }
    this->orbit_delta = wrap(rs::mul_add(1.0f - retained, step, this->orbit_delta * retained));
    const float half = this->orbit_delta * 0.5f;
    const F4 axis = {0.0f, trigonometry::sin(half), 0.0f, trigonometry::cos(half)};
    this->orbit_position = add(position, rotate(axis, sub(this->orbit_position, position)));
    const F4 from = horizontal(sub(this->orbit_position, position));
    const F4 previous = horizontal(sub(input.camera_position, position));
    const F4 from_direction = normalize_safe(from);
    const F4 previous_direction = normalize_safe(previous);
    const F4 normal = cross(from_direction, previous_direction);
    if (dot(from_direction, previous_direction) < rs::from_bits(0x3f5db22d)) {
        const float magnitude = length(normal);
        if (magnitude >= 0.001f) {
            const float inverse = refined_reciprocal(magnitude);
            const float half_angle = -rs::from_bits(0x3f060a92) * 0.5f;
            const float sine = trigonometry::sin(half_angle);
            const F4 rotation_axis = {
                normal[0] * inverse * sine,
                normal[1] * inverse * sine,
                normal[2] * inverse * sine,
                trigonometry::cos(half_angle),
            };
            this->orbit_position = add(position, rotate(rotation_axis, previous));
        } else {
            this->orbit_position = input.camera_position;
        }
    }
    const F4 relative = sub(position, this->orbit_position);
    const float distance = length(relative);
    if (distance > 0.001f) {
        this->orbit_position = sub(position, rs::map(relative, [&](float v) { return v * (2.0f / distance); }));
    }
    this->headings[6] = heading(horizontal(sub(position, this->orbit_position)));
}

}  // namespace s3::camera
