// Port of skate-core/src/player/offboard/ground_lifecycle.rs and its collision.rs,
// post.rs and publication.rs submodules.
#include "s3/core/player/offboard/ground_lifecycle.hpp"

#include "s3/core/physics/reciprocal_sqrt.hpp"

namespace s3::player::offboard::ground_lifecycle {

using detail::dot;
using detail::length;

float detail::length(Vector v) {
    const float q = dot(v, v);
    float r = physics::reciprocal_sqrt::estimate(q);
    for (int i = 0; i < 2; ++i) {
        r = rs::mul_add(r * 0.5f, rs::mul_add(-q, r * r, 1.0f), r);
    }
    return q == 0.0f ? 0.0f : q * r;
}

// ------------------------------------------------------------------ collision.rs
void check_collision(wipeout::Requests& requests, const CollisionSettings& s, const CollisionInput& i) {
    requests.mode = 4;
    if (dot(i.skeleton_velocity_16336, i.skeleton_velocity_16336) <= s.minimum_speed * s.minimum_speed) {
        return;
    }
    const wipeout::Frame& shared = *i.shared;
    const float requested = (shared.flags_2480 & 0x80u) != 0 ? s.special_scalar : 1.0f;
    // The source loads vehicle_scalar whenever the contact flag is present,
    // even when vehicle_contact comparison does not emit reason7.
    float vehicle = 1.0f;
    if (i.contact_flag_4072) {
        if (i.contact_force_4056 > s.vehicle_contact) {
            requests.request(7, 0.0f);
        }
        vehicle = s.vehicle_scalar;
    }
    const float scalar = requested - vehicle >= 0.0f ? vehicle : requested;
    if (shared.maximum_pose_error > scalar * s.maximum_squash) {
        requests.request(18, 0.0f);
    } else {
        const float limit = s.maximum_displacement * scalar;
        if (dot(shared.pose_error, shared.pose_error) > limit * limit) {
            requests.request(1, 0.0f);
        } else if (wipeout::regional_force(shared, s.maximum_body_contact * scalar,
                                           s.maximum_arm_contact * vehicle)) {
            requests.request(0, 0.0f);
        }
    }
}

// ------------------------------------------------------------------ post.rs
void post_physics(ground_entry::State& state, wipeout::Requests& requests, const CollisionSettings& settings,
                  const PostInput& input) {
    check_collision(requests, settings, input.collision);
    if ((input.processed_flags_2484 & 0x400u) != 0) {
        if (state.flags_144_to_150[6] && state.flags_144_to_150[2]) {
            requests.request(31, 0.0f);
        }
        state.elapsed_168 = 0.0f;
        if (state.flags_144_to_150[2] || state.angle_172 > rs::from_bits(0x3fc90fdbu) ||
            input.ground_kind_356 == 2 || input.ground_kind_356 == 4 || 0.5f > state.frame_80[1][1]) {
            requests.request(31, 0.0f);
        } else if (!(dot(input.processed_velocity_608, input.skeleton_displacement_16304) >=
                     length(input.processed_velocity_608) * rs::from_bits(0xbca3d70au))) {
            requests.request(32, 0.0f);
        }
    } else {
        const float a = rs::abs(dot(input.skeleton_displacement_16304, state.frame_80[1]));
        const float b = rs::abs(dot(input.skeleton_displacement_16288, state.frame_80[1]));
        if (a > rs::from_bits(0x3eb33333u) || b > rs::from_bits(0x3e19999au)) {
            state.elapsed_168 += rs::from_bits(0x3c888889u);
        } else {
            state.elapsed_168 = 0.0f;
        }
        if (!(state.elapsed_168 < rs::from_bits(0x3d4cccceu))) {
            requests.request(32, 0.0f);
        }
    }
    if (state.flags_144_to_150[1]) {
        requests.request(28, 0.0f);
    }
}

// ------------------------------------------------------------------ publication.rs
Publication publish(const ground_entry::State& state, const PublicationInput& i) {
    const bool near = i.ground_flags_752_to_754[1] || !(state.distance_164 >= rs::from_bits(0x3daaaaabu));
    const bool blocked = state.flags_144_to_150[3] || (i.contact_flags_368 & 8u) != 0;
    const float distance =
        i.ground_flags_752_to_754[0] && (i.contact_flags_368 & 1u) != 0
            ? length(rs::from_fn<4>([&](std::size_t n) { return i.contact_position_192[n] - i.query_position_816[n]; }))
            : rs::from_bits(0x501502f9u);
    const float projection = dot(i.motion_up_864, i.motion_vector_1040);
    Publication p;
    p.physics_counter_36 = state.counter_152;
    p.physics_counter_40 = state.counter_156;
    p.physics_flag_86 = (i.processed_flags_2476 & 0x00400000u) != 0 && !state.flags_144_to_150[0];
    p.offboard_flag_304 = state.flags_144_to_150[0];
    p.offboard_kind_88 = i.ground_kind_356;
    p.offboard_scalar_112 = i.ground_scalar_360;
    p.offboard_flag_329 = blocked;
    p.offboard_flag_330 = near && !blocked;
    p.offboard_distance_116 = distance;
    p.offboard_flag_334 = i.ground_flags_752_to_754[2];
    p.offboard_scalar_32 = state.flags_144_to_150[2] ? 1.0f : 0.0f;
    p.offboard_flag_328 = state.flags_144_to_150[2];
    p.animation_vector_144 =
        rs::from_fn<4>([&](std::size_t n) { return i.motion_vector_1040[n] - i.motion_up_864[n] * projection; });
    p.animation_flag_164 = true;
    p.offboard_hand_flags_306_307 =
        rs::map(i.hand_flags, [](const std::array<bool, 3>& f) { return (f[0] && !f[1]) || f[2]; });
    return p;
}

}  // namespace s3::player::offboard::ground_lifecycle
