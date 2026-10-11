// Port of skate-core/src/animation/output/actor_packet.rs.
// Actor field publication825937EC..825939C8 and external payload copy82592810.
// Runtime services remain explicit source interfaces; their implementation is
// not supplied here and these fields do not manufacture a gameplay impulse.
//
// Traits with associated types: `PlayerPhysicsSettingsSource<Error>` and
// `ActorPublicationEnvironment<Error, Settings>` are abstract templates exposing
// `using Error` / `using Settings`; `publish<E>` accepts any E with that shape
// (deriving from the abstract template is the intended way).
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/animation/output/packet_reset.hpp"
#include "s3/core/animation/output/physics_packet.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::output::actor_packet {

using packet_reset::AdditionalResetFields;
using physics_packet::PhysicsPosePacket;

struct ExternalPhysicsInput {
    /// Ten complete vectors; word representation preserves non-arithmetic bits.
    std::array<std::array<std::uint32_t, 4>, 10> vectors{};
    std::uint32_t flags = 0;

    ///82592810 copies160 bytes and only the high seven flag bits. Remaining
    /// destination flag bits and native padding are not overwritten.
    void copy_from(const ExternalPhysicsInput& source) {
        vectors = source.vectors;
        flags = (flags & 0x01ffffffu) | (source.flags & 0xfe000000u);
    }
};
S3_DERIVE_EQ(ExternalPhysicsInput, vectors, flags)

struct ExternalReset {
    std::array<std::array<std::uint32_t, 4>, 4> transform{};
    /// Source payload+64 -> packet10768; retains the complete byte.
    std::uint8_t byte64 = 0;
};
S3_DERIVE_EQ(ExternalReset, transform, byte64)

struct ActorPacketFields {
    std::array<std::uint32_t, 4> external_impulse{};
    ExternalPhysicsInput external_physics;
    ExternalReset external_reset;
    /// Actor1904 bit29 -> packet10784; not reset by82590028.
    bool actor_flag_1904_bit29 = false;
};

struct ActorPublicationState {
    std::uint32_t flags1904 = 0;
    std::uint32_t flags1908 = 0;
    bool external_controller_present = false;
    std::array<std::uint32_t, 4> external_impulse{};
    ExternalPhysicsInput external_physics;
    ExternalReset external_reset;
    /// Actor1768 selects the native168-byte per-player settings record.
    std::uint32_t player_index = 0;
};

/// A source-owned binding to the settings object captured before Actor+56's
/// virtual call. Each read uses the same captured object and168-byte stride.
template <class Error_>
struct PlayerPhysicsSettingsSource {
    using Error = Error_;
    virtual ~PlayerPhysicsSettingsSource() = default;
    /// Record+160. This read is skipped by the fixed-settings branch.
    virtual rs::Result<float, Error> truck_tightness(std::uint32_t player_index) = 0;
    /// Record+164. This read is skipped by the fixed-settings branch.
    virtual rs::Result<float, Error> wheel_hardness(std::uint32_t player_index) = 0;
    /// Record+172. Read after either branch has written both output scalars.
    virtual rs::Result<std::uint32_t, Error> requested_physics_mode(std::uint32_t player_index) = 0;
};

/// Exact unresolved external service boundaries for this field publisher.
/// A game adapter must implement these from their source-owned systems.
/// `Settings_` must provide the PlayerPhysicsSettingsSource<Error_> methods (by value).
template <class Error_, class Settings_>
struct ActorPublicationEnvironment {
    /// Implementations must report unavailable source services as an error.
    /// There is deliberately no default or placeholder implementation.
    using Error = Error_;
    using Settings = Settings_;
    virtual ~ActorPublicationEnvironment() = default;
    ///8259FC28: global830CFD94 ->220 ->328 virtual12. None means no current
    /// object. Otherwise property824639E8(object20), including its native
    /// default, supplies the enum. Property identifier is-616069660.
    virtual rs::Result<std::optional<std::int32_t>, Error> current_scene_mode() = 0;
    /// Read global byte830B7C30 verbatim, without bool normalization.
    virtual rs::Result<std::uint8_t, Error> ignore_respawn_reset_button() = 0;
    /// Actor subobject+56 virtual16, low return byte. Flags25/28 have already
    /// been consumed when called. Concrete vtable binding remains unresolved.
    virtual rs::Result<std::uint8_t, Error> actor_subobject56_slot16(ActorPublicationState& actor) = 0;
    /// Capture global83067068 at82593954 before slot16 at8259395C. The binding
    /// must stay valid across that callback and the subsequent field reads.
    /// Do not snapshot record values here: native reads them after the callback.
    virtual rs::Result<Settings, Error> capture_physics_settings() = 0;
};

namespace detail {
inline void replace_flag(std::uint32_t& flags, std::uint32_t bit, bool value) {
    flags = (flags & ~(1u << bit)) | (static_cast<std::uint32_t>(value) << bit);
}
}  // namespace detail

/// Full actor-owned suffix on distinct actor/packet payload storage. Scene
/// lookup is skipped for an external controller or force-braking bit26.
/// This stops before the physical object's borrowed-packet setter82DB3C48.
template <class E>
rs::Result<rs::Unit, typename E::Error> publish(ActorPublicationState& actor, PhysicsPosePacket& pose,
                                                AdditionalResetFields& reset, ActorPacketFields& fields,
                                                E& environment) {
    reset.actor_flag_1904_bit23 = (actor.flags1904 & (1u << 23)) != 0;
    reset.actor_flag_1908_bit2 = (actor.flags1908 & (1u << 2)) != 0;
    reset.external_impulse_active = (actor.flags1904 & (1u << 30)) != 0;
    fields.external_impulse = actor.external_impulse;
    fields.external_physics.copy_from(actor.external_physics);
    reset.external_physics_input_active = (actor.flags1904 & (1u << 27)) != 0;
    bool scene_mode_flag = false;
    if (!actor.external_controller_present && (actor.flags1904 & (1u << 26)) == 0) {
        auto mode = environment.current_scene_mode();
        if (mode.is_err()) return rs::make_err(mode.unwrap_err());
        const std::optional<std::int32_t> value = mode.unwrap();
        scene_mode_flag = value && (*value == 19 || *value == 20);
    }
    detail::replace_flag(pose.flags, 22, scene_mode_flag);
    reset.externally_controlled = actor.external_controller_present;
    fields.external_reset = actor.external_reset;
    fields.actor_flag_1904_bit29 = (actor.flags1904 & (1u << 29)) != 0;
    reset.prevent_manual_respawn = (actor.flags1904 & (1u << 28)) != 0;
    reset.force_braking = (actor.flags1904 & (1u << 26)) != 0;
    detail::replace_flag(pose.flags, 24, (actor.flags1904 & (1u << 25)) != 0);
    actor.flags1904 &= ~(1u << 25);
    {
        auto ignore = environment.ignore_respawn_reset_button();
        if (ignore.is_err()) return rs::make_err(ignore.unwrap_err());
        reset.ignore_respawn_reset_button = ignore.unwrap();
    }
    actor.flags1904 &= ~(1u << 28);
    auto captured = environment.capture_physics_settings();
    if (captured.is_err()) return rs::make_err(captured.unwrap_err());
    auto& settings = captured.unwrap();
    auto slot16 = environment.actor_subobject56_slot16(actor);
    if (slot16.is_err()) return rs::make_err(slot16.unwrap_err());
    const bool use_fixed_settings = slot16.unwrap() != 0;
    if (use_fixed_settings) {
        reset.truck_tightness = rs::from_bits(0x3f333333u);
        reset.wheel_hardness = rs::from_bits(0x3f333333u);
    } else {
        auto truck = settings.truck_tightness(actor.player_index);
        if (truck.is_err()) return rs::make_err(truck.unwrap_err());
        reset.truck_tightness = truck.unwrap();
        auto wheel = settings.wheel_hardness(actor.player_index);
        if (wheel.is_err()) return rs::make_err(wheel.unwrap_err());
        reset.wheel_hardness = wheel.unwrap();
    }
    auto mode = settings.requested_physics_mode(actor.player_index);
    if (mode.is_err()) return rs::make_err(mode.unwrap_err());
    reset.requested_physics_mode = mode.unwrap();
    return rs::Unit{};
}

}  // namespace s3::animation::output::actor_packet
