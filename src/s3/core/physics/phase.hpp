// Port of skate-core/src/physics/phase.rs.
//
// Engine-independent transport records for one authoritative simulation tick.
//
// These records deliberately contain requests and published values rather
// than references to a game/world runtime. Game code owns when they are
// consumed; core code owns their shape and deterministic validation.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "s3/core/math.hpp"
#include "s3/core/player/state.hpp"

namespace s3::physics::phase {

using math::Vector3;
using player::state::PhysicalStateId;

enum class PhysicsBody {
    Board,
    Rider,
};

/// Rust `enum PhysicsCommand`; each variant is a nested struct held in `value`.
struct PhysicsCommand {
    struct SetVelocity {
        PhysicsBody body = PhysicsBody::Board;
        Vector3 linear;
        Vector3 angular;
    };
    struct ApplyImpulse {
        PhysicsBody body = PhysicsBody::Board;
        Vector3 impulse;
        Vector3 point;
    };
    /// Tuple variant `RequestState(PhysicalStateId)`; `_0` is Rust's `.0`.
    struct RequestState {
        PhysicalStateId _0 = PhysicalStateId::PhysicsGround;
    };
    struct SetContactMode {
        PhysicsBody body = PhysicsBody::Board;
        bool enabled = false;
    };

    std::variant<SetVelocity, ApplyImpulse, RequestState, SetContactMode> value;

    PhysicsCommand(SetVelocity v) : value(v) {}
    PhysicsCommand(ApplyImpulse v) : value(v) {}
    PhysicsCommand(RequestState v) : value(v) {}
    PhysicsCommand(SetContactMode v) : value(v) {}
};
S3_DERIVE_EQ(PhysicsCommand::SetVelocity, body, linear, angular)
S3_DERIVE_EQ(PhysicsCommand::ApplyImpulse, body, impulse, point)
S3_DERIVE_EQ(PhysicsCommand::RequestState, _0)
S3_DERIVE_EQ(PhysicsCommand::SetContactMode, body, enabled)
S3_DERIVE_EQ(PhysicsCommand, value)

class PhysicsCommandBuffer {
public:
    static PhysicsCommandBuffer new_(std::uint64_t tick) { return PhysicsCommandBuffer(tick); }
    std::uint64_t tick() const { return tick_; }
    const std::vector<PhysicsCommand>& commands() const { return commands_; }
    bool is_empty() const { return commands_.empty(); }

    rs::Result<rs::Unit, std::string> push(std::uint64_t tick, PhysicsCommand command);
    rs::Result<rs::Unit, std::string> clear(std::uint64_t tick);

    bool operator==(const PhysicsCommandBuffer& o) const {
        return tick_ == o.tick_ && commands_ == o.commands_;
    }
    bool operator!=(const PhysicsCommandBuffer& o) const { return !(*this == o); }

private:
    explicit PhysicsCommandBuffer(std::uint64_t tick) : tick_(tick) {}
    std::uint64_t tick_;
    std::vector<PhysicsCommand> commands_;
};

/// Rust `enum PhysicsEvent`; each variant is a nested struct held in `value`.
struct PhysicsEvent {
    struct StateChanged {
        PhysicalStateId from = PhysicalStateId::PhysicsGround;
        PhysicalStateId to = PhysicalStateId::PhysicsGround;
    };
    struct Landing {};
    struct Wipeout {};
    struct Contact {
        PhysicsBody body = PhysicsBody::Board;
    };

    std::variant<StateChanged, Landing, Wipeout, Contact> value;

    PhysicsEvent(StateChanged v) : value(v) {}
    PhysicsEvent(Landing v) : value(v) {}
    PhysicsEvent(Wipeout v) : value(v) {}
    PhysicsEvent(Contact v) : value(v) {}
};
S3_DERIVE_EQ(PhysicsEvent::StateChanged, from, to)
S3_DERIVE_EQ_EMPTY(PhysicsEvent::Landing)
S3_DERIVE_EQ_EMPTY(PhysicsEvent::Wipeout)
S3_DERIVE_EQ(PhysicsEvent::Contact, body)
S3_DERIVE_EQ(PhysicsEvent, value)

class PhysicsEventBuffer {
public:
    static PhysicsEventBuffer new_(std::uint64_t tick) { return PhysicsEventBuffer(tick); }
    std::uint64_t tick() const { return tick_; }
    const std::vector<PhysicsEvent>& events() const { return events_; }

    rs::Result<rs::Unit, std::string> emit(std::uint64_t tick, PhysicsEvent event);

    bool operator==(const PhysicsEventBuffer& o) const {
        return tick_ == o.tick_ && events_ == o.events_;
    }
    bool operator!=(const PhysicsEventBuffer& o) const { return !(*this == o); }

private:
    explicit PhysicsEventBuffer(std::uint64_t tick) : tick_(tick) {}
    std::uint64_t tick_;
    std::vector<PhysicsEvent> events_;
};

/// Compact immutable publication consumed by animation, camera, rendering,
/// feedback and UI. Detailed native records remain owned by their subsystems;
/// this is the stable cross-phase summary.
struct PhysicalOutputSnapshot {
    std::uint64_t tick = 0;
    PhysicalStateId state = PhysicalStateId::PhysicsGround;
    Vector3 board_position;
    Vector3 board_linear_velocity;
    Vector3 rider_root_position;
    Vector3 rider_linear_velocity;
    /// Ground/contact results are published once by the solver and must not
    /// be reconstructed by camera, animation, or feedback consumers.
    Vector3 ground_normal;
    std::uint32_t contact_count = 0;
    Vector3 predicted_position;
    bool grounded = false;
    bool wiping_out = false;
    bool landed = false;
    /// Animation-authored foot contact strength captured before its one-tick publication reset.
    float footstep_strength = 0.0f;
    std::int32_t footstep_bone = 0;
    float foot_push_speed = 0.0f;
    /// Offboard foot-placement support flags (state56 `+105` per foot, left then right) from
    /// the native ground conditioner. Only meaningful in BipedGround.
    std::array<bool, 2> feet_supported{};
    std::vector<PhysicsEvent> events;
};
S3_DERIVE_EQ(PhysicalOutputSnapshot, tick, state, board_position, board_linear_velocity,
             rider_root_position, rider_linear_velocity, ground_normal, contact_count,
             predicted_position, grounded, wiping_out, landed, footstep_strength, footstep_bone,
             foot_push_speed, feet_supported, events)

}  // namespace s3::physics::phase
