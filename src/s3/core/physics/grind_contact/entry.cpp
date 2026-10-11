// Port of skate-core/src/physics/grind_contact/entry.rs.
#include "s3/core/physics/grind_contact/entry.hpp"

#include "s3/core/physics/grind_contact/arithmetic.hpp"

namespace s3::physics::grind_contact::entry {

using admission::engagement_slope_sine;

namespace {
V cross_(V a, V b) {
    return V{
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}
}  // namespace

Output Engagement::update(const Input& input) {
    if (input.valid && input.kind == 2 && (input.previous_state_2504 == 701 || input.previous_state_2504 == 403) &&
        input.speed > 1.8f) {
        tipslide_frames = 8;
    } else {
        const std::uint32_t next = tipslide_frames - 1u;
        tipslide_frames = static_cast<std::int32_t>(next) < 0 ? 0 : next;
    }
    Output out;
    out.valid = input.valid;
    out.flags = (input.flags & ~0x02000000u) | (tipslide_frames > 0 ? 0x02000000u : 0u);
    out.entry_velocity = input.previous_entry_velocity;
    out.impact_speed = 0.0f;
    if (!out.valid) {
        return out;
    }
    out.flags &= ~0x40000000u;
    if (!(input.category == 100 || input.category == 200)) {
        return out;
    }
    if (rs::abs(dot3(input.up, input.direction)) > 0.5f) {
        out.valid = false;
        if (input.category == 200) {
            out.wipeout_reasons.push_back(13);
        }
        return out;
    }
    out.flags |= 0x40000000u;
    const V initial = input.category == 100 ? input.board_velocity : input.air_velocity;
    out.entry_velocity = input.category == 100
                             ? grounded_velocity(input.kind, input.direction, input.board_velocity,
                                                 input.balance_2720, input.speed, *input.vertical_help)
                             : airborne_velocity(input.kind, input.direction, input.normal, input.air_velocity);
    V delta;
    for (std::size_t i = 0; i < 4; ++i) delta[i] = out.entry_velocity[i] - initial[i];
    out.impact_speed = arithmetic::square_root(dot3(delta, delta));
    // MeasureEngagement runs for BOTH ground and air accepted entries.
    if (out.impact_speed > input.max_delta) {
        out.wipeout_reasons.push_back(9);
    }
    const float along = dot3(input.board_velocity, input.direction);
    V transverse;
    for (std::size_t i = 0; i < 4; ++i) transverse[i] = input.direction[i] * along - input.board_velocity[i];
    transverse[1] = 0.0f;
    if (dot3(transverse, transverse) > 49.0f &&
        (input.surface_kind == 0 || dot3(input.board_velocity, input.high_side) > 0.0f)) {
        out.wipeout_reasons.push_back(14);
    }
    if (!out.wipeout_reasons.empty()) {
        out.valid = false;
    }
    return out;
}

V grounded_velocity(std::uint32_t kind, V direction, V velocity, float balance, float speed,
                    const point_graph::PointGraph<4>& vertical_help) {
    float base;
    if (kind == 1) {
        base = 0.1f;
    } else if ((kind == 2 || kind == 4) && balance != 0.0f && speed < 1.45f) {
        base = 1.0f;
    } else if (kind == 2) {
        base = 0.4f;
    } else if (kind == 0 || kind == 3 || kind == 4 || kind == 5) {
        base = 0.2f;
    } else {
        base = 0.0f;
    }
    const float help = vertical_help.evaluate(engagement_slope_sine(direction, velocity));
    float amount = rs::mul_add(help, 1.0f - base, base);
    // Native fsel on (1-amount), including unordered selecting1.
    amount = 1.0f - amount >= 0.0f ? amount : 1.0f;
    const float along = dot3(velocity, direction);
    V out;
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = rs::mul_add(direction[i] * along, amount, velocity[i] * (1.0f - amount));
    }
    return out;
}

V airborne_velocity(std::uint32_t kind, V direction, V normal, V velocity) {
    const V across = cross_(normal, direction);
    const float d = dot3(velocity, across);
    V removed;
    for (std::size_t i = 0; i < 4; ++i) removed[i] = across[i] * d;
    float amount;
    switch (kind) {
        case 0:
        case 3:
            amount = 0.4f;
            break;
        case 1:
        case 5:
            amount = 0.2f;
            break;
        case 2:
        case 4:
            amount = 0.8f;
            break;
        default:
            amount = 0.0f;
            break;
    }
    V out;
    for (std::size_t i = 0; i < 4; ++i) {
        out[i] = rs::mul_add(velocity[i] - removed[i], amount, velocity[i] * (1.0f - amount));
    }
    return out;
}

std::vector<std::size_t> airborne_rejections(V direction, V up, V board_velocity, V air_velocity, V corrected,
                                             std::uint32_t surface_kind, V surface_side, float max_delta) {
    if (rs::abs(dot3(up, direction)) > 0.5f) {
        return {13};
    }
    V delta;
    for (std::size_t i = 0; i < 4; ++i) delta[i] = corrected[i] - air_velocity[i];
    std::vector<std::size_t> reasons;
    if (arithmetic::square_root(dot3(delta, delta)) > max_delta) {
        reasons.push_back(9);
    }
    const float along = dot3(board_velocity, direction);
    V transverse;
    for (std::size_t i = 0; i < 4; ++i) transverse[i] = direction[i] * along - board_velocity[i];
    transverse[1] = 0.0f;
    if (dot3(transverse, transverse) > 49.0f && (surface_kind == 0 || dot3(board_velocity, surface_side) > 0.0f)) {
        reasons.push_back(14);
    }
    return reasons;
}

}  // namespace s3::physics::grind_contact::entry
