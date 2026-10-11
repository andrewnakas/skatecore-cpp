// Port of skate-game/src/physics/animation_phase_packet.rs
// (mounted as `animation_phase::packet` via #[path]).
//! Owned PlayerInput packet storage; original actor publication825937EC.
#include "s3/game/physics/animation_phase_packet.hpp"

namespace s3::game::physics::animation_phase::packet {

using sc::animation::output::packet_reset::RESET_POSE;

namespace {
std::array<std::array<std::uint32_t, 4>, 4> reset_pose_bits() {
    std::array<std::array<std::uint32_t, 4>, 4> out{};
    for (std::size_t c = 0; c < 4; ++c) out[c] = rs::to_bits(RESET_POSE[c]);
    return out;
}
}  // namespace

rs::Result<AnimationProfile, std::string> AnimationProfile::load(const sd::Collections& data, std::string_view mode) {
    // Original82DB1B08..1B78 initializes these variant slots in this order.
    static const char* const MODES[] = {"easy", "normal", "hardcore", "motorized", "test"};
    std::optional<std::uint32_t> physics_mode;
    for (std::uint32_t i = 0; i < 5; ++i) {
        if (mode == MODES[i]) {
            physics_mode = i;
            break;
        }
    }
    if (!physics_mode) {
        return rs::make_err(fmt::cat("Undefined player physics profile ", std::string(mode)));
    }
    AnimationProfile out;
    // 82BBBDE0: Globals400/layout96, bound by828A0154.
    S3G_TRY(out.skitch_transition_time, data.float_("anim_skitching", "default", "LongSkitchIntoReachTime"));
    // 82BA5150 global316/layout1844;8289FB24 binds pushing.
    S3G_TRY(out.maximum_ground_angle_degrees, data.float_("anim_motion", "pushing", "disable_push_brake_at_slope"));
    // Initial equipment preferences use the fixed settings value from
    // 82593988/94. The host allows users to change these independently.
    out.truck_tightness = rs::from_bits(0x3f333333u);
    out.wheel_hardness = rs::from_bits(0x3f333333u);
    out.physics_mode = *physics_mode;
    out.prevent_manual_respawn = false;
    out.ignore_respawn_reset_button = 0;
    out.force_braking = false;
    out.suppress_transition = false;
    out.gesture_selections = std::nullopt;
    // 82B988B8 returns false when its optional context is absent.
    out.suppress_up_gesture = false;
    out.gesture_force_brake_bypass = false;
    return out;
}

AnimationPhaseOutput AnimationPhaseOutput::new_() {
    AnimationPhaseOutput out;
    // Reserved storage: native reset writes every field before use.
    auto& r = out.reset;
    r.compression = 0.0f;
    r.foot_ik_influence = {0.0f, 0.0f};
    r.next_step_position_valid = false;
    r.actor_flag_1904_bit23 = false;
    r.actor_flag_1908_bit2 = false;
    r.external_impulse_active = false;
    r.external_physics_input_active = false;
    r.externally_controlled = false;
    r.prevent_manual_respawn = false;
    r.ignore_respawn_reset_button = 0;
    r.force_braking = false;
    r.truck_tightness = 0.0f;
    r.wheel_hardness = 0.0f;
    r.auxiliary_vectors = {};
    r.requested_physics_mode = 1;
    auto& p = out.publication;
    p.stance_byte = 0;
    p.timestep = 0.0f;
    p.scalar_10388 = 0.0f;
    p.flags_10375_10496_10784 = {0, 0, 0};
    p.vector_10480 = {0, 0, 0, 0};
    p.matrix_10704 = reset_pose_bits();
    p.byte_10768 = 0;
    p.truck_tightness = 0.0f;
    p.scalar_10792 = 0.0f;
    p.flag_10371 = 0;
    // No external impulse/controller/reset provider in this game. The
    // inactive payloads are owned storage and are not consumed.
    out.external.vectors = {};
    out.external.flags = 0;
    out.mirrored = 0;
    out.weight_forwards = 0;
    out.flags = 0;
    return out;
}

void AnimationPhaseOutput::publish(const sc::animation::output::physics_packet::PhysicsPosePacket& pose,
                                   const AnimationProfile& profile, std::uint32_t actor_flags) {
    auto& r = reset;
    // No external impulse provider: Actor1904 bit23 is inactive.
    r.actor_flag_1904_bit23 = false;
    r.actor_flag_1908_bit2 = profile.suppress_transition || (actor_flags & (1u << 2)) != 0;
    r.truck_tightness = profile.truck_tightness;
    r.wheel_hardness = profile.wheel_hardness;
    r.requested_physics_mode = profile.physics_mode;
    r.prevent_manual_respawn = profile.prevent_manual_respawn;
    r.ignore_respawn_reset_button = profile.ignore_respawn_reset_button;
    r.force_braking = profile.force_braking;
    // Free skating has no challenge scene19/20 requesting bit22.
    // 82593924 reads Actor1904 bit25, not the input-filter word1908.
    flags = pose.flags & ~((1u << 24) | (1u << 22));
    mirrored = pose.mirrored ? 1 : 0;
    weight_forwards = pose.weight_forwards ? 1 : 0;
    sc::input::animation_packet::AnimationPacketFields p{};
    p.stance_byte = pose.board_flipped ? 1 : 0;
    p.timestep = pose.timestep;
    p.scalar_10388 = r.compression;
    p.flags_10375_10496_10784 = {
        std::uint8_t(r.actor_flag_1904_bit23 ? 1 : 0),
        0,
        0,  // Actor1904 bit29 is published only by the canonical reset reply.
    };
    p.vector_10480 = {0, 0, 0, 0};
    p.matrix_10704 = reset_pose_bits();
    p.byte_10768 = 0;
    p.truck_tightness = r.truck_tightness;
    p.scalar_10792 = r.wheel_hardness;
    p.flag_10371 = pose.riding_switch ? 1 : 0;
    publication = p;
}

void AnimationPhaseOutput::publish_external_reset(sc::animation::output::actor_packet::ExternalReset reply) {
    publication.matrix_10704 = reply.transform;
    publication.byte_10768 = reply.byte64;
    publication.flags_10375_10496_10784[2] = 1;
}

sc::player::input_phase::AnimationInputPacket AnimationPhaseOutput::packet() const {
    const auto& r = reset;
    std::array<std::array<std::uint32_t, 4>, 6> vectors{};
    for (std::size_t i = 0; i < 6; ++i) vectors[i] = rs::to_bits(r.auxiliary_vectors[i]);
    return sc::player::input_phase::AnimationInputPacket{
        publication,
        external,
        std::uint8_t(r.next_step_position_valid ? 1 : 0),
        mirrored,
        weight_forwards,
        std::uint8_t(r.actor_flag_1908_bit2 ? 1 : 0),
        0,
        0,
        std::uint8_t(r.prevent_manual_respawn ? 1 : 0),
        r.ignore_respawn_reset_button,
        std::uint8_t(r.force_braking ? 1 : 0),
        vectors[0],
        vectors[1],
        vectors[2],
        vectors[3],
        vectors[4],
        vectors[5],
        r.foot_ik_influence[0],
        r.foot_ik_influence[1],
        r.requested_physics_mode,
        flags,
    };
}

}  // namespace s3::game::physics::animation_phase::packet
