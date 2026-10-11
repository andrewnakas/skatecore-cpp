// Port of skate-core/src/player/wipeout/ground.rs (whole file).
#include "s3/core/player/wipeout/ground.hpp"

#include "s3/core/physics/native_arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::wipeout::ground {

using physics::native_arithmetic::dot3;
using physics::native_arithmetic::vector_max;
using physics::native_arithmetic::vector_min;

void check(Requests& state, const Settings& s, const Mode& mode, const Frame& f) {
    state.enter_ground();
    if (state.cooldown > 0.0f) {
        state.cooldown -= f.timestep;
        return;
    }
    const GroundSettings& g = s.ground;
    const float vehicle = f.group_8 ? g.vehicle_scalar : 1.0f;
    const float trick = (f.flags_2472 & (1u << 15)) != 0 ? rs::from_bits(0x3e99999au) : 1.0f;
    const float skitch = (f.flags_2476 & 8u) != 0 ? g.skitch_scalar : 1.0f;
    const float arms = (f.flags_2476 & 8u) != 0 ? g.skitch_arms_scalar : 1.0f;
    const float skater =
        ((f.flags_2472 & (1u << 28)) == 0 && f.maximum_skater_force > 0.0f) ? g.skater_scalar : 1.0f;
    const float scale = ((vehicle * trick) * skitch) * skater;
    const float squash = (f.flags_2476 & (1u << 30)) != 0 ? g.max_squash_coffin : g.max_squash;
    const float displacement = g.max_displacement * scale;
    if ((mode.check_squash || f.time_on_ground > rs::from_bits(0x3d4ccccdu)) &&
        f.maximum_pose_error > vehicle * squash) {
        state.request(18, 0.0f);
    } else if (dot3(f.pose_error, f.pose_error) > displacement * displacement) {
        state.request(1, 0.0f);
    } else if (common::force(f, g.max_contact * scale, g.max_arm_contact * arms)) {
        state.request(0, 0.0f);
    }
    common::vehicle(state, s, f);
    if ((f.flags_2484 & (1u << 21)) == 0) {
        const float cosine = vector_min(
            vector_min(dot3(f.deck[0], f.input_board[0]), dot3(f.deck[1], f.input_board[1])),
            dot3(f.deck[2], f.input_board[2]));
        if (trigonometry::acos(vector_min(vector_max(cosine, -1.0f), 1.0f)) > g.max_deck_error) {
            state.request(3, 0.0f);
        }
    }
    float xz;
    float y;
    if ((f.flags_2472 & (1u << 15)) != 0) {
        xz = s.air.xz_trick;
        y = s.air.y_trick;
    } else {
        xz = mode.ground_xz;
        y = g.y_acceleration;
        if ((f.board_material_flags & (1u << 30)) != 0) {
            const float scalar = (f.flags_2472 & (1u << 28)) != 0 ? g.ai_scalar : g.player_scalar;
            xz *= scalar;
            y *= scalar;
        }
        if ((f.board_material_flags & (1u << 29)) != 0) {
            xz *= g.light_dmo_scalar;
        }
        if ((f.flags_2476 & 8u) != 0) {
            xz *= g.skitch_acc_scalar;
            y *= g.skitch_acc_scalar;
        }
        if ((f.flags_2484 & (1u << 21)) != 0) {
            xz = 0.0f;
            y = 0.0f;
        }
    }
    common::closing(state, f, xz, y);
    const float skitch_gate = (f.flags_2476 & (1u << 23)) != 0 ? 0.0f : 1.0f;
    const float slide_gate = (f.flags_2468 & (1u << 5)) != 0 ? 0.0f : 1.0f;
    const float speed = (g.balance_min_speed - f.speed) / g.balance_min_speed;
    const float amount = (speed * (1.0f - f.animation_up[1])) * slide_gate;
    const float delta = rs::mul_add(amount, skitch_gate, -g.balance_base);
    state.balance = delta > 0.0f ? state.balance + delta : 0.0f;
    if (state.balance > g.balance_total) {
        state.request(11, 0.0f);
    }
    if (f.conflicting) {
        state.request(16, 0.0f);
    }
    common::leaning(state, s, f);
}

void check_animation(Requests& state, const Settings& s, const Mode& mode, const Frame& f, float scale) {
    check(state, s, mode, f);
    if (common::force(f, s.air.max_contact, s.air.max_arm_contact)) {
        state.request(0, 0.0f);
    }
    if (f.opposing_contact > s.ground.opposing_contact * scale) {
        state.request(20, 0.0f);
    }
}

void check_plant(Requests& state, const Settings& s, const Frame& f) {
    state.mode = 3;
    if (f.maximum_pose_error > s.ground.max_squash) {
        state.request(18, 0.0f);
    } else if (common::length(f.pose_error) > s.air.max_displacement) {
        state.request(1, 0.0f);
    } else if (common::force(f, s.air.max_contact, s.air.max_arm_contact)) {
        state.request(0, 0.0f);
    }
    common::closing(state, f, s.air.xz_trick, s.air.y_trick);
}

}  // namespace s3::player::wipeout::ground
