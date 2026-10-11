// Port of skate-game/src/graph_host/motion_nodes.rs.
//! Stock MotionGraph configuration. Native constructor defaults are kept here;
//! lifecycle execution and persistent state are owned by MotionHost.
#pragma once
#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/riding_fakie.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/result.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/graph_host/motion/body_tweak.hpp"
#include "s3/game/graph_host/motion_air_leg.hpp"
#include "s3/game/graph_host/motion_conditions.hpp"
#include "s3/game/graph_host/motion_grind.hpp"
#include "s3/game/graph_host/motion_hand_services.hpp"
#include "s3/game/graph_host/motion_hooks.hpp"
#include "s3/game/graph_host/motion_intent_filter.hpp"
#include "s3/game/graph_host/motion_kickturn.hpp"
#include "s3/game/graph_host/motion_landing.hpp"
#include "s3/game/graph_host/motion_native.hpp"
#include "s3/game/graph_host/motion_reset.hpp"
#include "s3/game/graph_host/motion_riding.hpp"
#include "s3/game/graph_host/motion_runout.hpp"
#include "s3/game/graph_host/motion_scoring_trick.hpp"
#include "s3/game/graph_host/motion_shove.hpp"
#include "s3/game/graph_host/motion_sliding.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"
#include "s3/game/graph_host/motion_tricks.hpp"
#include "s3/game/graph_host/motion_twist_lean.hpp"
#include "s3/game/graph_host/motion_wipeout.hpp"
#include "s3/game/graph_host/pushing.hpp"

namespace s3::game::graph_host::motion_nodes {

using motion_conditions::MotionCondition;
using pushing::PushOperation;
using s3::animation::output::attributes::AttributeName;
using s3::animation::playback::PlayAnimation;
using s3::animation::playback::TransitionSettings;
using s3::data::state_graph_attributes::Attributes;
using s3::data::state_graph_binding::Node;
using s3::data::state_graph_binding::OperationKind;

/// Rust `enum MotionOperation`; each variant is a nested struct held in `value`.
/// Tuple variants hold their payload in `_0`.
struct MotionOperation {
    struct Grind {
        motion_grind::Operation _0;
    };
    struct Trick {
        motion_tricks::Operation _0;
    };
    struct SetDeckPitchAndYaw {
        AttributeName yaw;
        AttributeName pitch;
    };
    struct Play {
        PlayAnimation _0;
    };
    ///TU3 vtable82309664: Begin/Update/End all point to the empty82B61BB8.
    struct PrintText2D {};
    struct ToggleBoard {};
    struct Runout {
        motion_runout::Operation _0;
    };
    struct AttachIntent {
        std::string intent;
        AttributeName attribute;
        bool set = false;
    };
    struct Push {
        PushOperation _0;
    };
    /// Angle, direction, quickness, speed, holding, in native emission order.
    struct SetTurning {
        std::array<AttributeName, 5> _0;
    };
    struct ApplyingBodyTilt {};
    struct SettingBodyTilt {
        AttributeName _0;
    };
    struct Crouching {
        AttributeName _0;
    };
    struct Pumping {};
    struct BipedCadence {};
    struct MatchCadence {};
    struct OffboardBodyTweakBlend {
        motion::body_tweak::Settings _0;
    };
    struct DisallowPumping {};
    struct FakieHeadChannel {};
    struct Native {
        motion_native::Operation _0;
    };
    struct Slide {
        motion_sliding::Operation _0;
    };
    struct KickTurn {
        motion_kickturn::Operation _0;
    };
    struct BodySpin {};
    struct AirLeg {
        motion_air_leg::Operation _0;
    };
    struct ClearTrickAttr {};
    struct ScoringTrick {
        motion_scoring_trick::Operation _0;
    };
    struct SetBumpCoefficients {
        AttributeName x;
        AttributeName y;
    };
    struct Landing {
        motion_landing::Operation _0;
    };
    struct Wipeout {
        motion_wipeout::Operation _0;
    };
    struct TwistLean {
        motion_twist_lean::Operation _0;
    };
    struct HandService {
        motion_hand_services::Operation _0;
    };
    struct IntentFilter {
        motion_intent_filter::Operation _0;
    };
    struct CharacterGesture {};
    struct EndGesture {};
    struct Shove {
        motion_shove::ShoveOperation _0;
    };
    struct ResetAnimation {
        motion_reset::Operation _0;
    };
    struct UpdateRidingFakie {
        s3::animation::riding_fakie::Settings _0;
    };
    /// Authored stock operations whose state is owned by the physical/gameplay
    /// producers rather than by the animation controller. Keeping these as a
    /// typed family lets the authored graph retain its native operation set;
    /// execution reports a missing producer instead of silently dropping the
    /// node.
    struct StockGameplay {
        motion_stock_gameplay::Operation _0;
    };
    struct SetSpeed {
        AttributeName name;
        std::optional<float> value;
    };
    struct Hook {
        motion_hooks::MotionHook _0;
    };
    struct Riding {
        motion_riding::RidingOperation _0;
    };
    struct Condition {
        MotionCondition _0;
    };
    struct Unsupported {
        OperationKind kind = OperationKind::Behavior;
        std::string name;
    };

    using Value = std::variant<Grind, Trick, SetDeckPitchAndYaw, Play, PrintText2D, ToggleBoard, Runout, AttachIntent,
                               Push, SetTurning, ApplyingBodyTilt, SettingBodyTilt, Crouching, Pumping, BipedCadence,
                               MatchCadence, OffboardBodyTweakBlend, DisallowPumping, FakieHeadChannel, Native, Slide,
                               KickTurn, BodySpin, AirLeg, ClearTrickAttr, ScoringTrick, SetBumpCoefficients, Landing,
                               Wipeout, TwistLean, HandService, IntentFilter, CharacterGesture, EndGesture, Shove,
                               ResetAnimation, UpdateRidingFakie, StockGameplay, SetSpeed, Hook, Riding, Condition,
                               Unsupported>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    MotionOperation(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }
};
S3_DERIVE_EQ(MotionOperation::Grind, _0)
S3_DERIVE_EQ(MotionOperation::Trick, _0)
S3_DERIVE_EQ(MotionOperation::SetDeckPitchAndYaw, yaw, pitch)
S3_DERIVE_EQ(MotionOperation::Play, _0)
S3_DERIVE_EQ_EMPTY(MotionOperation::PrintText2D)
S3_DERIVE_EQ_EMPTY(MotionOperation::ToggleBoard)
S3_DERIVE_EQ(MotionOperation::Runout, _0)
S3_DERIVE_EQ(MotionOperation::AttachIntent, intent, attribute, set)
S3_DERIVE_EQ(MotionOperation::Push, _0)
S3_DERIVE_EQ(MotionOperation::SetTurning, _0)
S3_DERIVE_EQ_EMPTY(MotionOperation::ApplyingBodyTilt)
S3_DERIVE_EQ(MotionOperation::SettingBodyTilt, _0)
S3_DERIVE_EQ(MotionOperation::Crouching, _0)
S3_DERIVE_EQ_EMPTY(MotionOperation::Pumping)
S3_DERIVE_EQ_EMPTY(MotionOperation::BipedCadence)
S3_DERIVE_EQ_EMPTY(MotionOperation::MatchCadence)
S3_DERIVE_EQ(MotionOperation::OffboardBodyTweakBlend, _0)
S3_DERIVE_EQ_EMPTY(MotionOperation::DisallowPumping)
S3_DERIVE_EQ_EMPTY(MotionOperation::FakieHeadChannel)
S3_DERIVE_EQ(MotionOperation::Native, _0)
S3_DERIVE_EQ(MotionOperation::Slide, _0)
S3_DERIVE_EQ(MotionOperation::KickTurn, _0)
S3_DERIVE_EQ_EMPTY(MotionOperation::BodySpin)
S3_DERIVE_EQ(MotionOperation::AirLeg, _0)
S3_DERIVE_EQ_EMPTY(MotionOperation::ClearTrickAttr)
S3_DERIVE_EQ(MotionOperation::ScoringTrick, _0)
S3_DERIVE_EQ(MotionOperation::SetBumpCoefficients, x, y)
S3_DERIVE_EQ(MotionOperation::Landing, _0)
S3_DERIVE_EQ(MotionOperation::Wipeout, _0)
S3_DERIVE_EQ(MotionOperation::TwistLean, _0)
S3_DERIVE_EQ(MotionOperation::HandService, _0)
S3_DERIVE_EQ(MotionOperation::IntentFilter, _0)
S3_DERIVE_EQ_EMPTY(MotionOperation::CharacterGesture)
S3_DERIVE_EQ_EMPTY(MotionOperation::EndGesture)
S3_DERIVE_EQ(MotionOperation::Shove, _0)
S3_DERIVE_EQ(MotionOperation::ResetAnimation, _0)
S3_DERIVE_EQ(MotionOperation::UpdateRidingFakie, _0)
S3_DERIVE_EQ(MotionOperation::StockGameplay, _0)
S3_DERIVE_EQ(MotionOperation::SetSpeed, name, value)
S3_DERIVE_EQ(MotionOperation::Hook, _0)
S3_DERIVE_EQ(MotionOperation::Riding, _0)
S3_DERIVE_EQ(MotionOperation::Condition, _0)
S3_DERIVE_EQ(MotionOperation::Unsupported, kind, name)
S3_DERIVE_EQ(MotionOperation, value)

/// `impl OperationFactory for MotionFactory` (Instance = MotionOperation, Error = String), shaped
/// for `s3::data::state_graph_binding::Binding::instantiate_operations`.
struct MotionFactory {
    s3::data::Result<std::optional<MotionOperation>> create(OperationKind kind, Node parent, const Attributes& a);
    s3::data::Status add_parameter(MotionOperation& instance, const Attributes& a);
};

/// `pub(super) fn transition`.
TransitionSettings transition(const Attributes& a);

}  // namespace s3::game::graph_host::motion_nodes
