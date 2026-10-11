// Port of skate-core/src/physics/grind_contact/balance.rs.
#include "s3/core/physics/grind_contact/balance.hpp"

#include "s3/core/physics/grind_contact/arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::grind_contact::balance {

namespace {
float magnitude(V value) { return arithmetic::square_root(dot3(value, value)); }

V blend(V a, float a_weight, V b, float b_weight) {
    V out;
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(a[i], a_weight, b[i] * b_weight);
    return out;
}

V retain_side(V previous, V side) { return dot3(previous, side) > 0.0f ? side : negate(side); }

}  // namespace

V negate(V value) { return V{-value[0], -value[1], -value[2], -value[3]}; }

///82C1E220 and82D86BC0..C70: quaternion half-angle rotation, not a
/// Rodrigues replacement with a different operation order.
V rotate(V axis, V value, float angle) {
    const auto [sin, cos] = trigonometry::sin_cos(angle * 0.5f);
    const V q = scale(axis, sin);
    const V first_cross = cross(q, value);
    V intermediate;
    for (std::size_t i = 0; i < 4; ++i) intermediate[i] = rs::mul_add(value[i], cos, first_cross[i]);
    const V second_cross = cross(q, intermediate);
    V out;
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(second_cross[i], 2.0f, value[i]);
    return out;
}

void BalanceState::update_target_up(TargetUpInput input, const BalanceContact* contact,
                      BalanceVectors& vectors) {
    if (contact == nullptr) return;
    const GrindSurface& surface = *contact->surface;
    V normal;
    if (input.category != 400) {
        normal = input.board_up;
    } else if (input.previous_state == 402 || input.previous_state == 404) {
        normal = blend(this->previous_normal, 0.85f, surface.upmost_normal, rs::from_bits(0x3e199998u));
    } else {
        normal = blend(this->previous_normal, 0.925f, surface.tilted_upmost_normal, rs::from_bits(0x3d999998u));
    }
    const V direction = contact->directed_grind_direction;
    const V projected = cross(cross(direction, normal), direction);
    const float length = magnitude(projected);
    vectors.grind_normal = length <= 0.001f ? surface.upmost_normal : scale(projected, arithmetic::reciprocal(length));
    this->previous_normal = vectors.grind_normal;

    const V target = blend(input.board_up, 0.8f, surface.upmost_normal, rs::from_bits(0x3e4cccccu));
    // Unlike the projected-normal and rotation-axis branches, the original
    // has NO zero-length fallback here. Do not silently invent one.
    vectors.target_up = scale(target, arithmetic::reciprocal(magnitude(target)));
    const float alignment = rs::min(rs::max(dot3(vectors.grind_normal, vectors.target_up), -1.0f), 1.0f);
    const float max_angle = rs::from_bits(0x3e7a35ddu);
    if (trigonometry::acos(alignment) > max_angle) {
        const V axis = cross(vectors.grind_normal, vectors.target_up);
        const float axis_length = magnitude(axis);
        vectors.target_up = axis_length <= 0.001f
                                ? vectors.grind_normal
                                : rotate(scale(axis, arithmetic::reciprocal(axis_length)), vectors.grind_normal,
                                         max_angle);
    }
}

float BalanceState::update_exit_lean(ExitLeanInput input, const BalanceContact* contact,
                       const point_graph::PointGraph<8>& exit_lean_angle_vs_time, BalanceVectors& vectors) {
    const bool active = input.category == 400 || input.current_state == 701 || contact != nullptr;
    this->frames_away = active ? 0 : static_cast<std::int32_t>(static_cast<std::uint32_t>(this->frames_away) + 1u);
    bool droppable = false;
    if (contact != nullptr) {
        if (contact->kind == 2) {
            droppable = active && contact->surface->kind != GeometryType::ThinRail;
        } else {
            droppable = contact->surface->kind == GeometryType::Ledge;
        }
    }
    bool started = false;
    //82D86880..94's carry/sign sequence implements a signed >20 test.
    if (this->frames_away > 20) {
        this->exit_angle_degrees = 0.0f;
        this->elapsed = 0.0f;
        this->entry_delay = 2.0f;
    } else if (this->elapsed > 0.0f || (active && droppable && contact != nullptr)) {
        if (this->elapsed == 0.0f) {
            // Only a real contact can start the timer in this branch.
            if (contact != nullptr) {
                switch (contact->entry_kind) {
                    case EntryKind::RideFromBelow:
                        this->entry_delay = 0.8f;
                        break;
                    case EntryKind::RideIntoCoping:
                        this->entry_delay = 0.1f;
                        break;
                    default:
                        this->entry_delay = 2.0f;
                        break;
                }
            }
            started = true;
        }
        if (input.grind_substate != 2) {
            this->elapsed = input.timestep + this->elapsed;
        }
    }
    if (this->elapsed <= 0.0f) {
        return this->exit_angle_degrees;
    }
    const float time = this->elapsed - this->entry_delay;
    this->exit_angle_degrees = time <= 0.0f ? 0.0f : exit_lean_angle_vs_time.evaluate(time);
    if (contact != nullptr) {
        const GrindSurface& surface = *contact->surface;
        if (surface.kind == GeometryType::ThinRail) {
            this->exit_direction =
                retain_side(this->exit_direction, cross(surface.upmost_normal, contact->primitive_direction));
        } else if (started) {
            this->exit_direction = surface.high_side;
        } else {
            this->exit_direction = retain_side(this->exit_direction, surface.high_side);
        }
        if (this->exit_angle_degrees > 0.0f) {
            const V direction = contact->directed_grind_direction;
            const V axis = cross(direction, this->exit_direction)[1] > 0.0f ? direction : negate(direction);
            const float radians = this->exit_angle_degrees * rs::from_bits(0x3c8efa35u);
            // Both outputs rotate; previous_normal deliberately does not.
            vectors.target_up = rotate(axis, vectors.target_up, radians);
            vectors.grind_normal = rotate(axis, vectors.grind_normal, radians);
        }
    }
    return this->exit_angle_degrees;
}

}  // namespace s3::physics::grind_contact::balance
