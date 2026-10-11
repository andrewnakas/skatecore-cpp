// Port of skate-core/src/player/offboard/surface_frame.rs (whole file).
#include "s3/core/player/offboard/surface_frame.hpp"

#include "s3/core/player/offboard/surface_frame/math.hpp"

namespace s3::player::offboard::surface_frame {

using namespace surface_frame::math;

void State::update_surface(const SurfaceInput& input) {
    if ((input.flags & 1u) == 0) return;
    source_normal = input.normal;
    Vector target = input.normal;
    if ((input.flags & 4u) != 0) {
        const Vector delta = sub(input.edge_point, input.origin);
        const Vector right = normalize_or(cross(UP, delta), Vector{});
        const Vector candidate = normalize_or(cross(delta, right), input.normal);
        const float limit = 45.0f * rs::from_bits(0x3c8efa35u);
        const Vector first = clamp_angle(candidate, input.normal, limit);
        const Vector second = clamp_angle(input.edge_normal, input.normal, limit);
        const Vector lateral = scale(right, dot(right, input.normal));
        target = normalize_or(sub(input.normal, lateral), UP);
        if (first[1] > target[1]) target = first;
        if (second[1] > target[1]) target = second;
        const float remaining = clamp(1.0f - dot(lateral, lateral), 0.0f, 1.0f);
        target = madd(target, square_root(remaining), lateral);
    }
    const float angle = rs::abs(wrap_angle(unsigned_angle(surface_normal, target)));
    if (angle < rs::from_bits(0x3f490fdbu) || target[1] > rs::from_bits(0x3f35c28fu)) {
        surface_normal = normalize_or(madd(surface_normal, 0.5f, scale(target, 0.5f)), surface_normal);
    }
}

void State::update_spring(const SpringInput& input) {
    const float blend = clamp(length(input.velocity) * 0.125f, 0.0f, 1.0f);
    const Vector candidate = madd(surface_normal, blend, scale(UP, 1.0f - blend));
    const Vector limited = clamp_angle(candidate, surface_normal, 36.0f * rs::from_bits(0x3c8efa35u));
    const Vector target = normalize_or(limited, input.up);
    Vector desired_delta = scale(sub(target, spring_normal), rs::from_bits(0x3e4ccccdu));
    desired_delta =
        clamp_axis(desired_delta, input.spring_right, rs::from_bits(0xbccccccdu), rs::from_bits(0x3ccccccdu));
    Vector acceleration = sub(desired_delta, spring_delta);
    acceleration = clamp_axis(acceleration, input.spring_right, rs::from_bits(0xbbc49ba6u), rs::from_bits(0x3bc49ba6u));
    acceleration =
        clamp_axis(acceleration, input.spring_forward, rs::from_bits(0xbd4ccccdu), rs::from_bits(0x3d4ccccdu));
    spring_delta = add(spring_delta, acceleration);
    spring_normal = normalize_or(add(spring_normal, spring_delta), spring_normal);
    const Vector lean_target =
        input.suppress_lean
            ? Vector{}
            : madd(input.forward, input.forward_delta * rs::from_bits(0x3d23d70au),
                   scale(input.right, input.right_delta * rs::from_bits(0x3f333333u)));
    lean = madd(lean_target, rs::from_bits(0x3d4ccccdu), scale(lean, rs::from_bits(0x3f733333u)));
    final_up = normalize_or(add(lean, spring_normal), spring_normal);
}

}  // namespace s3::player::offboard::surface_frame
