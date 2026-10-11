// Port of skate-core/src/player/wipeout/air.rs (whole file).
#include "s3/core/player/wipeout/air.hpp"

#include "s3/core/physics/native_arithmetic.hpp"

namespace s3::player::wipeout::air {

using physics::native_arithmetic::dot3;

namespace {
void bad_landing(Requests& state, const Settings& s, const Mode& mode, const Frame& f, bool use_com) {
    if (!mode.check_bad_landing || !f.board_contact) {
        return;
    }
    const V velocity = use_com ? f.com_velocity : f.deck_velocity;
    V normal = f.board_contact_normal;
    if (f.grind_selected) {
        if (f.grind_normal_valid) {
            normal = f.grind_normal;
        }
        if (-dot3(normal, velocity) > s.air.max_grind_speed * mode.bad_landing_scale) {
            state.request(8, 0.0f);
        }
    } else {
        const float into = -dot3(normal, velocity);
        const V tangent = rs::from_fn<4>([&](std::size_t i) { return rs::mul_add(normal[i], into, velocity[i]); });
        const float limit = s.air.max_landing_angle.evaluate(common::length(tangent)) * mode.bad_landing_scale;
        if (rs::abs(f.landing_angle) > limit) {
            state.request(5, 0.0f);
        }
        const float limit2 =
            (f.board_material_flags & (1u << 31)) != 0 ? s.air.max_stairs_speed : s.air.max_landing_speed;
        if (into > limit2 * mode.bad_landing_scale) {
            state.request(6, into);
        }
    }
}
}  // namespace

void check_collision(Requests& state, const Settings& s, const Mode& mode, const Frame& f) {
    if (mode.check_squash && f.maximum_pose_error > s.air.max_squash) {
        state.request(18, 0.0f);
    } else if (dot3(f.pose_error, f.pose_error) > s.air.max_displacement * s.air.max_displacement) {
        state.request(1, 0.0f);
    } else if (common::force(f, s.air.max_contact, s.air.max_arm_contact)) {
        state.request(0, 0.0f);
    }
}

void check(Requests& state, const Settings& s, const Mode& mode, const Frame& f, bool use_com) {
    if (state.mode != 2) {
        state.mode = 2;
        state.contact_frames = 0;
    }
    if (f.board_contact) {
        state.contact_frames = 4;
    }
    if (state.contact_frames > 0) {
        state.contact_frames -= 1;
    }
    if (state.cooldown > 0.0f) {
        state.cooldown -= f.timestep;
        return;
    }
    common::vehicle(state, s, f);
    const AirSettings& a = s.air;
    const bool body_flip = (f.flags_2476 & (1u << 26)) != 0;
    const float scalar = body_flip ? a.body_flip_scalar : 1.0f;
    const float deck_scalar = body_flip ? a.body_flip_acc_scalar : 1.0f;
    float xz;
    float y;
    if ((f.flags_2472 & (1u << 15)) != 0 && f.jump_fix_frames > a.ignore_danger_frames) {
        xz = a.xz_trick;
        y = a.y_trick;
    } else if (f.wheel_contact) {
        xz = mode.ground_xz * deck_scalar;
        y = s.ground.y_acceleration;
    } else {
        xz = a.xz_acceleration * deck_scalar;
        y = a.y_acceleration;
    }
    if ((f.board_material_flags & (1u << 29)) != 0) {
        xz *= a.light_dmo_scalar;
    }
    common::closing(state, f, xz, y);
    const float displacement = a.max_displacement * scalar;
    if (mode.check_squash && f.maximum_pose_error > a.max_squash * scalar) {
        state.request(18, 0.0f);
    } else if (dot3(f.pose_error, f.pose_error) > displacement * displacement) {
        state.request(1, 0.0f);
    } else if (common::force(f, a.max_contact * scalar, a.max_arm_contact)) {
        state.request(0, 0.0f);
    }
    if (f.flip_active && f.flip_requested_speed == 0.0f && f.system_up_y < 0.0f) {
        state.request(4, 0.0f);
    }
    bad_landing(state, s, mode, f, use_com);
    common::leaning(state, s, f);
    // Two independent sensitive conditions can request21 twice.
    if ((f.flags_2476 & (1u << 26)) != 0 && f.compliant) {
        state.request(21, 0.0f);
    }
    if ((f.flags_2472 & (1u << 6)) != 0 && (f.compliant || f.board_contact)) {
        state.request(21, 0.0f);
    }
}

}  // namespace s3::player::wipeout::air
