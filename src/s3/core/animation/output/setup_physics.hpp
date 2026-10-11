// Port of skate-core/src/animation/output/setup_physics.rs.
// Source-bound packet composition for concrete Actor::SetUpPhysics82593640.
// The caller supplies the runtime services and completed materialized poses.
// This is not the complete Actor update or Skeleton attribute interpreter.
#pragma once
#include <cstdint>
#include <variant>

#include "s3/core/animation/commands/batch.hpp"
#include "s3/core/animation/commands/buffers.hpp"
#include "s3/core/animation/output/actor_packet.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/output/intents.hpp"
#include "s3/core/animation/output/motion_graph_packet.hpp"
#include "s3/core/animation/output/packet_reset.hpp"
#include "s3/core/animation/output/physics_packet.hpp"
#include "s3/core/animation/slice.hpp"

namespace s3::animation::output::setup_physics {

using actor_packet::ActorPacketFields;
using actor_packet::ActorPublicationState;
using attributes::AnimationAttribute;
using attributes::MotionGraphAttribute;
using attributes::PacketAttributes;
using commands::batch::CompletedSetData;
using commands::buffers::BufferError;
using intents::IntentMap;
using motion_graph_packet::MotionGraphPacket;
using packet_reset::AdditionalResetFields;
using physics_packet::ActorPoseBuffers;
using physics_packet::PhysicsPosePacket;
using physics_packet::SkaterPublicationState;

/// Views cover fields touched by this concrete publication chain. Packet fields
/// only used by later Skeleton consumers are not fabricated here.
struct PhysicsInputPacket {
    PhysicsPosePacket pose;
    AdditionalResetFields reset;
    MotionGraphPacket motion_graph;
    IntentMap intents;
    PacketAttributes attributes;
    ActorPacketFields actor;
};

struct AnimationPublication {
    SkaterPublicationState& state;
    const CompletedSetData& completed;
    const ActorPoseBuffers& poses;
    Slice<const std::uint8_t> signal_name;
    Slice<const AnimationAttribute> tree_attributes;
};

/// Rust `enum IntentPublication<'a> { MotionGraph(&'a IntentMap), RetainPacket }`.
struct IntentPublication {
    struct MotionGraph {
        const IntentMap* _0 = nullptr;
    };
    /// Exact same-container branch825936FC; no clear/reinsert occurs.
    struct RetainPacket {};
    std::variant<MotionGraph, RetainPacket> value;
    IntentPublication(MotionGraph v) : value(v) {}
    IntentPublication(RetainPacket v) : value(v) {}
};

struct MotionGraphPublication {
    const MotionGraphPacket& state;
    IntentPublication intents;
    Slice<const MotionGraphAttribute> attributes;
};

/// Rust `enum PublicationError<E> { Pose(BufferError), RuntimeService(E) }`.
template <class E>
struct PublicationError {
    struct Pose {
        BufferError _0;
    };
    struct RuntimeService {
        E _0;
    };
    std::variant<Pose, RuntimeService> value;
    PublicationError(Pose v) : value(std::move(v)) {}
    PublicationError(RuntimeService v) : value(std::move(v)) {}

    friend bool operator==(const Pose& a, const Pose& b) { return a._0 == b._0; }
    friend bool operator==(const RuntimeService& a, const RuntimeService& b) { return a._0 == b._0; }
    bool operator==(const PublicationError& o) const { return value == o.value; }
    bool operator!=(const PublicationError& o) const { return !(*this == o); }
};

///82DB3C48 only stores a borrowed packet pointer at physical+1808. This safe
/// binding keeps the entire owned packet alive and prevents its next mutation
/// while a consumer reads it. It does not execute the downstream physics step.
class PhysicsPacketBinding {
public:
    explicit PhysicsPacketBinding(const PhysicsInputPacket& packet) : packet_(&packet) {}
    const PhysicsInputPacket& packet() const { return *packet_; }

private:
    const PhysicsInputPacket* packet_;
};

/// Concrete canonical virtual144 is a no-op. Remaining phases execute in the
/// native order: reset, SkaterAnim, MG, intents, MG/tree attrs, actor suffix,
/// then packet binding. Failure preserves writes already made and returns no
/// binding; the caller must not consume the partially published packet.
template <class E>
rs::Result<PhysicsPacketBinding, PublicationError<typename E::Error>> compose(
    PhysicsInputPacket& packet, AnimationPublication animation, MotionGraphPublication graph,
    ActorPublicationState& actor, E& environment) {
    using Error = PublicationError<typename E::Error>;
    {
        auto r = packet_reset::reset(packet.pose, packet.reset);
        if (r.is_err()) return rs::make_err(Error(typename Error::Pose{r.unwrap_err()}));
    }
    {
        auto r = physics_packet::publish(animation.state, animation.completed, animation.poses, packet.pose,
                                         static_cast<double>(rs::from_bits(0x3c888889u)),
                                         animation.signal_name);
        if (r.is_err()) return rs::make_err(Error(typename Error::Pose{r.unwrap_err()}));
    }
    motion_graph_packet::publish(graph.state, packet.motion_graph);
    if (const auto* source = std::get_if<IntentPublication::MotionGraph>(&graph.intents.value)) {
        packet.intents.replace_from(*source->_0);
    }
    packet.attributes.replace_from(graph.attributes, animation.tree_attributes);
    {
        auto r = actor_packet::publish(actor, packet.pose, packet.reset, packet.actor, environment);
        if (r.is_err()) return rs::make_err(Error(typename Error::RuntimeService{r.unwrap_err()}));
    }
    return PhysicsPacketBinding(packet);
}

}  // namespace s3::animation::output::setup_physics
