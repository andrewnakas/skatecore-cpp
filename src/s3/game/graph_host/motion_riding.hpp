// Port of skate-game/src/graph_host/motion_riding.rs (module `motion_riding`).
//! Native riding packet and timer leaves. The graph host owns these states;
//! physics/pose callers consume the resulting values in actor update order.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/animation/body_tilt.hpp"
#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_riding {

using motion_animation::MotionAnimation;
using s3::animation::output::attributes::AttributeName;
using s3::data::state_graph_attributes::Attributes;
using s3::graph::controller::Frame;

rs::Result<s3::animation::body_tilt::Settings, std::string> body_tilt_settings(const s3::data::Collections& data);

enum class ForceMode {
    AnimationData,
    PhysicsBoard,
    AnimationBoard,
};
/// `ForceMode::native_value`.
inline std::uint32_t native_value(ForceMode self) {
    switch (self) {
        case ForceMode::AnimationData: return 0;
        case ForceMode::PhysicsBoard: return 1;
        case ForceMode::AnimationBoard: return 2;
    }
    return 0;
}

/// Rust `enum RidingOperation`; each variant is a nested struct held in `value`.
struct RidingOperation {
    struct CreateAttribute {
        AttributeName name;
        std::array<std::optional<float>, 3> values{};
        bool set = false;
    };
    struct SetDistComToBoard {
        AttributeName name;
        bool manually = false;
        bool adjust_for_velocity = false;
    };
    struct ForcePhysics {
        ForceMode _0 = ForceMode::AnimationData;
    };
    struct TimeSinceTeleport {};
    struct TimeSinceKickturn {};
    struct ManualOutTimer {};
    struct SetManualOutTimer {
        float _0 = 0.0f;
    };

    std::variant<CreateAttribute, SetDistComToBoard, ForcePhysics, TimeSinceTeleport, TimeSinceKickturn,
                 ManualOutTimer, SetManualOutTimer>
        value;

    RidingOperation(CreateAttribute v) : value(v) {}
    RidingOperation(SetDistComToBoard v) : value(v) {}
    RidingOperation(ForcePhysics v) : value(v) {}
    RidingOperation(TimeSinceTeleport v) : value(v) {}
    RidingOperation(TimeSinceKickturn v) : value(v) {}
    RidingOperation(ManualOutTimer v) : value(v) {}
    RidingOperation(SetManualOutTimer v) : value(v) {}

    static std::optional<RidingOperation> parse(const Attributes& a);
};
S3_DERIVE_EQ(RidingOperation::CreateAttribute, name, values, set)
S3_DERIVE_EQ(RidingOperation::SetDistComToBoard, name, manually, adjust_for_velocity)
S3_DERIVE_EQ(RidingOperation::ForcePhysics, _0)
S3_DERIVE_EQ_EMPTY(RidingOperation::TimeSinceTeleport)
S3_DERIVE_EQ_EMPTY(RidingOperation::TimeSinceKickturn)
S3_DERIVE_EQ_EMPTY(RidingOperation::ManualOutTimer)
S3_DERIVE_EQ(RidingOperation::SetManualOutTimer, _0)
S3_DERIVE_EQ(RidingOperation, value)

struct RidingState {
    //8258F488 initializes full5912/5916; final reset825953B0 clears
    //specific3156/3224.
    float time_since_teleport = 0.0f;
    float time_since_kickturn = 0.0f;
    float manual_out_timer = 0.0f;

    ///Specific MotionGraph CA4 bit23, getter8258FB68/setter8258FB78.
    ///Constructor8258F5B8 clears it; Reset82595480 preserves it.
    bool dark = false;
    /// Specific MotionGraph+3156, GetLastGoodLandingVelocity8258F7C8.
    float last_good_landing_velocity = 0.0f;
    ///82BB2868 writes ProcessedPhysIn+16420 only on behavior Begin.
    ///None means no graph request has changed the physical mode yet.
    std::optional<ForceMode> force_mode;

    static RidingState new_() { return RidingState{}; }
    rs::Result<rs::Unit, std::string> execute(const RidingOperation& operation, std::uint8_t phase,
                                              const Frame& frame, MotionAnimation& animation,
                                              std::optional<std::uint32_t> physical_category,
                                              std::optional<float> animation_height, bool manualing);
};

}  // namespace s3::game::graph_host::motion_riding
