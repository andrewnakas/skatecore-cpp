// Port of skate-game/src/physics/animation_phase_packet.rs
// (mounted as `animation_phase::packet` via #[path]).
//! Owned PlayerInput packet storage; original actor publication825937EC.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "s3/core/animation/output/actor_packet.hpp"
#include "s3/core/animation/output/packet_reset.hpp"
#include "s3/core/animation/output/physics_packet.hpp"
#include "s3/core/input/animation_packet.hpp"
#include "s3/core/player/input_phase.hpp"
#include "s3/data/collections.hpp"
#include "s3/game/common.hpp"

namespace s3::game::physics::animation_phase::packet {

/// Host-owned player preferences, distinct from the stock response curves.
struct AnimationProfile {
    float maximum_ground_angle_degrees = 0.0f;
    float skitch_transition_time = 0.0f;
    float truck_tightness = 0.0f;
    float wheel_hardness = 0.0f;
    std::uint32_t physics_mode = 0;
    bool prevent_manual_respawn = false;
    std::uint8_t ignore_respawn_reset_button = 0;
    bool force_braking = false;
    bool suppress_transition = false;
    /// Optional gesture customization and external context are host policy.
    std::optional<std::array<std::uint32_t, 4>> gesture_selections;
    bool suppress_up_gesture = false;
    bool gesture_force_brake_bypass = false;

    static rs::Result<AnimationProfile, std::string> load(const sd::Collections& data, std::string_view mode);
};

struct AnimationPhaseOutput {
    /// pub(super)
    sc::animation::output::packet_reset::AdditionalResetFields reset{};
    // Private in Rust.
    sc::input::animation_packet::AnimationPacketFields publication{};
    sc::animation::output::actor_packet::ExternalPhysicsInput external{};
    std::uint8_t mirrored = 0;
    std::uint8_t weight_forwards = 0;
    std::uint32_t flags = 0;
    // Actor825938FC/393C consumes these request bits after publication.

    /// pub(super)
    static AnimationPhaseOutput new_();
    /// pub(super)
    void publish(const sc::animation::output::physics_packet::PhysicsPosePacket& pose,
                 const AnimationProfile& profile, std::uint32_t actor_flags);
    ///Actor825926F8 reply ->10704/10768/10784, then82DB5BE0 maps physical input.
    /// pub(super)
    void publish_external_reset(sc::animation::output::actor_packet::ExternalReset reply);
    /// The returned packet borrows `publication` and `external` (Rust `AnimationInputPacket<'_>`).
    sc::player::input_phase::AnimationInputPacket packet() const;
};

}  // namespace s3::game::physics::animation_phase::packet
