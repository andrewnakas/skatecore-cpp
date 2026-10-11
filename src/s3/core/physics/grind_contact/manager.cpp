// Port of skate-core/src/physics/grind_contact/manager.rs.
#include "s3/core/physics/grind_contact/manager.hpp"

#include "s3/core/physics/grind_contact/arithmetic.hpp"

namespace s3::physics::grind_contact::manager {

namespace {
///82D883E0: height DIFFERENCE chooses the high-side sample, not two positive heights.
bool is_backslash(const GeometryInput& i) {
    if (i.family != 2 || i.current_state == 402 || (i.geometry_flags & 0x80000000u) != 0) {
        return false;
    }
    const std::array<V, 2> offsets = {sub(i.far_points[0], i.point), sub(i.far_points[1], i.point)};
    const std::array<float, 2> heights = {dot3(offsets[0], i.upmost), dot3(offsets[1], i.upmost)};
    if (rs::abs(heights[0] - heights[1]) <= i.deck_to_truck * 0.1f) {
        return false;
    }
    const V high = heights[0] > heights[1] ? offsets[0] : offsets[1];
    return dot3(sub(i.board_position, i.point), high) > 0.0f;
}
}  // namespace

V directed_tangent(V start, V end, V velocity, V previous) {
    const V delta = sub(end, start);
    V direction = scale(delta, arithmetic::reciprocal(arithmetic::square_root(dot3(delta, delta))));
    const float speed = dot3(direction, velocity);
    if (speed < 0.0f) {
        direction = scale(direction, -1.0f);
    }
    if (rs::abs(speed) < 0.1f && dot3(previous, direction) < -0.9f) {
        direction = scale(direction, -1.0f);
    }
    return direction;
}

GeometryOutput tweak_geometry(GeometryInput i, std::uint32_t& avoid_five_o_frames) {
    GeometryOutput o{i.valid, i.family, i.flags};
    if (i.geometry_kind == 3 ||
        (i.family == 2 && (i.geometry_flags & 0x10000000u) != 0 && i.category != 400 && i.air_frames < 20) ||
        (i.family == 1 && (i.geometry_flags & 0x10000000u) != 0)) {
        o.valid = false;
    }
    if (o.valid && is_backslash(i)) {
        o.family = 4;
    }
    if (o.valid && i.previous_exit_angle > 0.0f && i.geometry_kind != 2 &&
        dot3(i.previous_exit_direction, i.high_side) < 0.0f) {
        o.valid = false;
    }
    if (o.valid && i.geometry_kind != 0) {
        V transverse = sub(scale(i.direction, dot3(i.direction, i.velocity)), i.velocity);
        transverse[1] = 0.0f;
        if (dot3(transverse, transverse) > 12.25f && dot3(i.velocity, i.high_side) < 0.0f) {
            o.valid = false;
        }
    }
    // Source executes this branch even if an earlier check rejected validity.
    if (o.family == 3 && i.geometry_kind == 2) {
        if (static_cast<std::int32_t>(avoid_five_o_frames) > 0) {
            o.valid = false;
        } else {
            const V hanging = (o.flags & 0x20000000u) != 0 ? scale(i.board_forward, -1.0f) : i.board_forward;
            if (dot3(hanging, i.high_side) > 0.0f) {
                const float vertical = dot3(hanging, i.upmost);
                o.flags = (o.flags & ~0x10000000u) | (vertical < 0.19f ? 0x10000000u : 0u);
                if (vertical < 0.0f) {
                    o.valid = false;
                    avoid_five_o_frames = 30;
                }
            }
        }
    }
    return o;
}

float gravity_relief(float& timer, bool valid, std::uint32_t category, V tangent, V velocity, float dt,
                     const point_graph::PointGraph<4>& vertical, const point_graph::PointGraph<4>& linear) {
    if (valid && category == 100 && admission::engagement_slope_sine(tangent, velocity) > 0.85f) {
        timer = vertical.evaluate(velocity[1]) * linear.evaluate(rs::abs(dot3(tangent, velocity)));
    }
    const float next = timer - dt;
    timer = -next >= 0.0f ? 0.0f : next;
    return timer;
}

Jumper Jumper::default_() {
    Jumper j;
    j.launched = false;
    j.cooldown = 0;
    j.family = 3;
    j.energy = 1.0f;
    j.geometry = JumpGeometry{
        0,
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
    };
    return j;
}

std::uint32_t Jumper::update(std::optional<JumpGeometry> geometry_in, std::uint32_t published_family) {
    launched = false;
    const std::uint32_t next = cooldown - 1u;
    cooldown = static_cast<std::int32_t>(next) < 0 ? 0 : next;
    if (geometry_in) {
        geometry = *geometry_in;
    }
    if (published_family != UINT32_MAX) {
        family = published_family;
    }
    const float e = energy + 0.0035f;
    energy = 1.0f - e >= 0.0f ? e : 1.0f;
    return cooldown > 0 ? 0x02000000u : 0u;
}

}  // namespace s3::physics::grind_contact::manager
