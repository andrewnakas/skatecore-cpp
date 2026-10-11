// Port of skate-game/src/graph_host/motion_conditions.rs.
//! Concrete MotionGraph condition leaves and stock attributes.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/core/animation/slice.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/core/graph/controller.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/input/gesture_catalog.hpp"
#include "s3/game/graph_host/condition_nodes.hpp"
#include "s3/game/graph_host/motion_dismount.hpp"
#include "s3/game/graph_host/motion_gameplay_conditions.hpp"
#include "s3/game/graph_host/motion_grind/conditions.hpp"
#include "s3/game/graph_host/motion_ground_slope.hpp"
#include "s3/game/graph_host/motion_landing.hpp"
#include "s3/game/graph_host/motion_offboard_cadence.hpp"
#include "s3/game/graph_host/motion_push_off.hpp"
#include "s3/game/graph_host/motion_riding_conditions.hpp"
#include "s3/game/graph_host/motion_stock_conditions.hpp"
#include "s3/game/graph_host/motion_stock_gameplay.hpp"
#include "s3/game/graph_host/motion_wipeout.hpp"
#include "s3/game/graph_runtime.hpp"

namespace s3::game::graph_host::motion_nodes {
struct MotionOperation;
}

namespace s3::game::graph_host::motion_conditions {

using s3::animation::output::attributes::AttributeName;
using s3::data::state_graph_attributes::Attributes;
using s3::graph::conditions::ActionCondition;
using s3::graph::conditions::Comparison;
using s3::graph::conditions::NumericCondition;
using s3::graph::controller::Frame;

/// Rust `enum MotionCondition`; each variant is a nested struct held in `value`.
/// Tuple variants hold their payload in `_0`.
struct MotionCondition {
    struct Grind {
        motion_grind::conditions::Condition _0;
    };
    struct HasTweak {
        NumericCondition _0;
    };
    struct CurrentGrabType {
        motion_stock_gameplay::GrabType _0;
    };
    struct ManualOutTimerIsActive {};
    struct Gesture {
        s3::game::input::gesture_catalog::Group _0;
    };
    struct DisableDismount {
        motion_dismount::Condition _0;
    };
    struct Landing {
        motion_landing::Condition _0;
    };
    struct Wipeout {
        motion_wipeout::Condition _0;
    };
    struct PushOff {
        motion_push_off::IsPushOffEnabled _0;
    };
    struct Shared {
        ActionCondition _0;
    };
    struct Intent {
        std::string name;
        bool filtered = false;
        NumericCondition numeric;
    };
    struct AnimationAttribute {
        AttributeName name;
        std::int32_t sequence_id = 0;
        NumericCondition numeric;
    };
    struct ExpireInTime {
        NumericCondition _0;
    };
    struct WillExpire {
        float in_time = 0.0f;
        std::optional<std::string> tag;
        bool wait_for_transitions = false;
    };
    struct InTimeWindow {
        float start = 0.0f;
        float length = 0.0f;
    };
    struct InStateForTime {
        std::string name;
        std::optional<std::size_t> target;
        NumericCondition numeric;
    };
    struct InParentStateForTime {
        std::optional<std::size_t> target;
        NumericCondition numeric;
    };
    struct ProSkater {
        AttributeName _0;
    };
    struct BreakOutOfPush {};
    struct ShouldLeaveSlide {
        bool right = false;
    };
    struct RidingGoofy {};
    struct RidingSwitch {};
    struct MongoPushFootTooFar {};
    struct LastState {
        std::string name;
        std::optional<std::size_t> target;
    };
    struct Gameplay {
        motion_gameplay_conditions::GameplayCondition _0;
    };
    struct Riding {
        motion_riding_conditions::MotionRidingCondition _0;
    };
    /// TimeToLand82BA7250: valid Air trajectory remaining time.
    struct TimeToLand {
        NumericCondition _0;
    };
    /// Native off-board trajectory time (PhysOutOffBoard+32).
    struct ObTimeToLand {
        NumericCondition _0;
    };
    /// Native off-board trajectory time used by dismount/runout branches.
    struct ObTrajTime {
        NumericCondition _0;
    };
    /// Biped locomotion bucket published by the physical off-board owner.
    struct LocoState {
        motion_offboard_cadence::LocoState _0;
    };
    struct GroundSlopeType {
        motion_ground_slope::GroundSlopeType _0;
    };
    /// Thin-ground branch of the stock BipedGround state.
    struct IsBipedGroundThin {};
    struct IsHoldingSkateboard {};
    /// Static gameplay has no moving actors yet; the stock condition is a
    /// resolved physical flag and must remain false until a moving support is
    /// published by the collision owner.
    struct IsStandingOnMovingObject {};
    /// IsInDebugAnimationsMode is a retail debug gate. TU3 has no gameplay
    /// producer for this flag; the shipped runtime never enables debug
    /// animation mode, so the condition is a resolved false leaf rather than
    /// an unsupported graph node.
    struct DebugAnimationsMode {};
    /// Stock gameplay predicates whose concrete values are published by the
    /// grind/handplant/skitch physical owners.
    struct StockGameplay {
        motion_stock_conditions::Condition _0;
    };

    using Value = std::variant<Grind, HasTweak, CurrentGrabType, ManualOutTimerIsActive, Gesture, DisableDismount,
                               Landing, Wipeout, PushOff, Shared, Intent, AnimationAttribute, ExpireInTime,
                               WillExpire, InTimeWindow, InStateForTime, InParentStateForTime, ProSkater,
                               BreakOutOfPush, ShouldLeaveSlide, RidingGoofy, RidingSwitch, MongoPushFootTooFar,
                               LastState, Gameplay, Riding, TimeToLand, ObTimeToLand, ObTrajTime, LocoState,
                               GroundSlopeType, IsBipedGroundThin, IsHoldingSkateboard, IsStandingOnMovingObject,
                               DebugAnimationsMode, StockGameplay>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    MotionCondition(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }

    static rs::Result<std::optional<MotionCondition>, std::string> parse(const Attributes& a);

    /// `evaluate(&self, host: &MotionHost, frame: &Frame)`. Generic over the MotionGraph host
    /// (graph_host::motion). The host fields read are those of Rust's MotionHost (see each arm).
    template <class MotionHost>
    rs::Result<bool, std::string> evaluate(const MotionHost& host, const Frame& frame) const;
};
S3_DERIVE_EQ(MotionCondition::Grind, _0)
S3_DERIVE_EQ(MotionCondition::HasTweak, _0)
S3_DERIVE_EQ(MotionCondition::CurrentGrabType, _0)
S3_DERIVE_EQ_EMPTY(MotionCondition::ManualOutTimerIsActive)
S3_DERIVE_EQ(MotionCondition::Gesture, _0)
S3_DERIVE_EQ(MotionCondition::DisableDismount, _0)
S3_DERIVE_EQ(MotionCondition::Landing, _0)
S3_DERIVE_EQ(MotionCondition::Wipeout, _0)
S3_DERIVE_EQ(MotionCondition::PushOff, _0)
S3_DERIVE_EQ(MotionCondition::Shared, _0)
S3_DERIVE_EQ(MotionCondition::Intent, name, filtered, numeric)
S3_DERIVE_EQ(MotionCondition::AnimationAttribute, name, sequence_id, numeric)
S3_DERIVE_EQ(MotionCondition::ExpireInTime, _0)
S3_DERIVE_EQ(MotionCondition::WillExpire, in_time, tag, wait_for_transitions)
S3_DERIVE_EQ(MotionCondition::InTimeWindow, start, length)
S3_DERIVE_EQ(MotionCondition::InStateForTime, name, target, numeric)
S3_DERIVE_EQ(MotionCondition::InParentStateForTime, target, numeric)
S3_DERIVE_EQ(MotionCondition::ProSkater, _0)
S3_DERIVE_EQ_EMPTY(MotionCondition::BreakOutOfPush)
S3_DERIVE_EQ(MotionCondition::ShouldLeaveSlide, right)
S3_DERIVE_EQ_EMPTY(MotionCondition::RidingGoofy)
S3_DERIVE_EQ_EMPTY(MotionCondition::RidingSwitch)
S3_DERIVE_EQ_EMPTY(MotionCondition::MongoPushFootTooFar)
S3_DERIVE_EQ(MotionCondition::LastState, name, target)
S3_DERIVE_EQ(MotionCondition::Gameplay, _0)
S3_DERIVE_EQ(MotionCondition::Riding, _0)
S3_DERIVE_EQ(MotionCondition::TimeToLand, _0)
S3_DERIVE_EQ(MotionCondition::ObTimeToLand, _0)
S3_DERIVE_EQ(MotionCondition::ObTrajTime, _0)
S3_DERIVE_EQ(MotionCondition::LocoState, _0)
S3_DERIVE_EQ(MotionCondition::GroundSlopeType, _0)
S3_DERIVE_EQ_EMPTY(MotionCondition::IsBipedGroundThin)
S3_DERIVE_EQ_EMPTY(MotionCondition::IsHoldingSkateboard)
S3_DERIVE_EQ_EMPTY(MotionCondition::IsStandingOnMovingObject)
S3_DERIVE_EQ_EMPTY(MotionCondition::DebugAnimationsMode)
S3_DERIVE_EQ(MotionCondition::StockGameplay, _0)
S3_DERIVE_EQ(MotionCondition, value)

/// Binds the containing-state targets of CurrentState/InStateForTime/LastState/
/// InParentStateForTime condition leaves once, against the immutable loaded hierarchy.
void bind_states(const graph_runtime::LoadedGraph& graph, s3::animation::Slice<motion_nodes::MotionOperation> operations);

namespace detail {
inline rs::Result<bool, std::string> to_string_err(rs::Result<bool, const char*> r) {
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return r.unwrap();
}
}  // namespace detail

template <class MotionHost>
rs::Result<bool, std::string> MotionCondition::evaluate(const MotionHost& host, const Frame& frame) const {
    using s3::animation::skeleton_input::name::encode;
    using R = rs::Result<bool, std::string>;
    auto err = [](const char* message) { return R(rs::make_err(std::string(message))); };
    if (const auto* c = std::get_if<Grind>(&value)) {
        if (!host.grind_conditions) return err("Grind condition requires completed physical output");
        return c->_0.evaluate(*host.grind_conditions);
    }
    // 82BA7760/82BA7848 compares the authored type through
    // ISkaterAnim; SetGrabType Begin/End writes that same owner.
    if (const auto* c = std::get_if<CurrentGrabType>(&value)) {
        return host.animation.grab_type == std::optional<motion_stock_gameplay::GrabType>(c->_0);
    }
    // TU3 82BA6B90 uses the filtered MG map (virtual +12),
    // requires at least one axis, then compares vector magnitude.
    if (const auto* c = std::get_if<HasTweak>(&value)) {
        const std::optional<float> x = host.animation.filtered_intent("TweakX");
        const std::optional<float> y = host.animation.filtered_intent("TweakY");
        return (x.has_value() || y.has_value()) &&
               (c->_0.comparison == Comparison::None ||
                c->_0.matches(rs::sqrt(rs::powi(x.value_or(0.0f), 2) + rs::powi(y.value_or(0.0f), 2))));
    }
    // Native 82BA78B0: strictly positive retained manual-out timer.
    if (is<ManualOutTimerIsActive>()) return host.riding.manual_out_timer > 0.0f;
    if (const auto* c = std::get_if<ShouldLeaveSlide>(&value)) {
        return static_cast<bool>(host.slide_latch.should_leave(c->right));
    }
    if (const auto* c = std::get_if<Gesture>(&value)) {
        return s3::game::input::gesture_catalog::group_has_intent(
            c->_0, [&](const char* name) { return host.action_controls.has(name); });
    }
    if (const auto* c = std::get_if<DisableDismount>(&value)) {
        const auto& push_brake = host.condition_inputs.push_brake;
        return detail::to_string_err(c->_0.evaluate(push_brake ? &*push_brake : nullptr));
    }
    if (const auto* c = std::get_if<Shared>(&value)) {
        return detail::to_string_err(
            c->_0.evaluate(host.condition_inputs, host.action_intents, frame.current, host.state_parents));
    }
    if (const auto* c = std::get_if<Gameplay>(&value)) return c->_0.evaluate(host);
    if (const auto* c = std::get_if<Riding>(&value)) return c->_0.evaluate(host);
    if (const auto* c = std::get_if<TimeToLand>(&value)) {
        if (!host.gameplay_conditions) return err("TimeToLand requires physical condition publication");
        const auto& p = *host.gameplay_conditions;
        return p.time_to_land_valid && c->_0.matches(p.time_to_land);
    }
    if (const auto* c = std::get_if<ObTimeToLand>(&value)) {
        if (!host.gameplay_conditions) return err("OBTimeToLand requires physical condition publication");
        return c->_0.matches(host.gameplay_conditions->offboard_time_to_land);
    }
    if (const auto* c = std::get_if<ObTrajTime>(&value)) {
        if (!host.gameplay_conditions) return err("OBTrajTime requires physical condition publication");
        const auto& p = *host.gameplay_conditions;
        return p.offboard_trajectory_valid && c->_0.matches(p.offboard_trajectory_time);
    }
    if (is<DebugAnimationsMode>()) return false;
    if (const auto* c = std::get_if<LocoState>(&value)) {
        if (!host.offboard_locomotion_state) {
            return err("LocoState requires retained Biped locomotion through OffBoard84");
        }
        return c->_0.evaluate(*host.offboard_locomotion_state);
    }
    if (const auto* c = std::get_if<GroundSlopeType>(&value)) {
        if (!host.ground_slope_type) return err("GroundSlopeType requires the completed ground slope publication");
        return c->_0.matches(*host.ground_slope_type);
    }
    if (is<IsBipedGroundThin>()) {
        if (!host.biped_ground_thin) return err("IsBipedGroundThin requires the native ground geometry publication");
        return static_cast<bool>(*host.biped_ground_thin);
    }
    if (is<IsHoldingSkateboard>()) {
        return host.toggle_board_physical.has_value() && host.toggle_board_physical->holding_board;
    }
    if (is<IsStandingOnMovingObject>()) {
        if (!host.gameplay_conditions) return err("IsStandingOnMovingObject requires the physical state publication");
        return static_cast<bool>(host.gameplay_conditions->moving_object);
    }
    if (const auto* c = std::get_if<StockGameplay>(&value)) return c->_0.evaluate(host);
    if (const auto* c = std::get_if<PushOff>(&value)) return c->_0.evaluate();
    if (const auto* c = std::get_if<Wipeout>(&value)) return c->_0.evaluate(host.wipeout_physical);
    if (const auto* c = std::get_if<Landing>(&value)) {
        std::optional<bool> override_preland;
        if (host.prelanding_physical) override_preland = host.prelanding_physical->override_prelanding(host.spin);
        return c->_0.evaluate(host.landing_physical, host.flags, override_preland);
    }
    if (const auto* c = std::get_if<LastState>(&value)) {
        //82BA4798 obtains the last state, then uses the same native
        //ancestor membership test as CurrentState82C13820.
        std::optional<std::size_t> cursor = frame.last;
        bool matched = false;
        if (c->target) {
            while (cursor) {
                const std::size_t state = *cursor;
                if (state == *c->target) {
                    matched = true;
                    break;
                }
                cursor = state < host.state_parents.size() ? host.state_parents[state] : std::nullopt;
            }
        }
        return matched;
    }
    if (const auto* c = std::get_if<Intent>(&value)) {
        const std::optional<float> v =
            c->filtered ? host.animation.filtered_intent(c->name) : host.animation.motion_intent(c->name);
        return v.has_value() && (c->numeric.comparison == Comparison::None || c->numeric.matches(*v));
    }
    if (const auto* c = std::get_if<AnimationAttribute>(&value)) {
        return condition_nodes::animation_attribute(host.animation.tree_attributes(), c->name, c->sequence_id,
                                                    c->numeric);
    }
    if (const auto* c = std::get_if<ExpireInTime>(&value)) {
        return c->_0.matches(host.animation.property().remaining_before_wrap);
    }
    if (const auto* c = std::get_if<WillExpire>(&value)) {
        //8296E988 uses authored InTime if tag is empty/component absent.
        float time = c->in_time;
        if (c->tag && host.time_tags) {
            const auto found = host.time_tags->find(*c->tag);
            if (found == host.time_tags->end()) {
                return R(rs::make_err("Missing MotionGraph float tag " + *c->tag));
            }
            time = found->second;
        }
        if (c->wait_for_transitions && host.animation.in_transition()) return false;
        const auto p = host.animation.property();
        return p.crossed_end || !(p.remaining_before_wrap > time);
    }
    if (const auto* c = std::get_if<InTimeWindow>(&value)) {
        if (host.animation.in_transition()) return false;
        const rs::Result<float, std::string> time = host.animation.current_time();
        if (time.is_err()) return rs::make_err(time.unwrap_err());
        return !(time.unwrap() < c->start) && !(time.unwrap() > c->start + c->length);
    }
    if (const auto* c = std::get_if<InStateForTime>(&value)) {
        return condition_nodes::state_time(frame, c->target, c->numeric);
    }
    if (const auto* c = std::get_if<InParentStateForTime>(&value)) {
        return condition_nodes::state_time(frame, c->target, c->numeric);
    }
    if (const auto* c = std::get_if<ProSkater>(&value)) return host.playback_context.pro_skater == c->_0;
    if (is<BreakOutOfPush>()) {
        //82BA6258: current tree time >= out factor * length, and
        // the push lifecycle has not latched continue_push.
        if (!host.push_state) return err("BreakOutOfPush requires initialized native push state");
        const auto& push = *host.push_state;
        const rs::Result<float, std::string> time = host.animation.current_time();
        if (time.is_err()) return rs::make_err(time.unwrap_err());
        const rs::Result<float, std::string> length = host.animation.current_length();
        if (length.is_err()) return rs::make_err(length.unwrap_err());
        return !(time.unwrap() < push.out_factor * length.unwrap()) && !push.continue_push;
    }
    if (is<RidingGoofy>()) {
        if (!host.physical_stance) return err("IsRidingGoofy requires PhysOutAnimation stance bytes");
        return std::get<0>(*host.physical_stance) == std::get<1>(*host.physical_stance);
    }
    if (is<RidingSwitch>()) {
        if (!host.playback_context.is_switch) return err("IsRidingSwitch requires actual relative stance");
        return static_cast<bool>(*host.playback_context.is_switch);
    }
    // MongoPushFootTooFar
    //82BA6378 queries the FIRST cached tree record via82D19010.
    //The event payload is a toe-bone FastString, not its weight.
    const AttributeName push_contact = encode("push_contact");
    for (const auto& event : host.animation.tree_attributes()) {
        if (!(event.name == push_contact)) continue;
        auto matches = [&](std::string_view name) {
            const AttributeName key = encode(name);
            for (std::size_t i = 0; i < 5; ++i) {
                if (event.payload._0[i] != std::optional<std::uint32_t>(key._0[i])) return false;
            }
            return true;
        };
        const bool right = matches("RightToeBase");
        const bool left = matches("LeftToeBase");
        if (!host.playback_context.is_mirrored) return err("MongoPushFootToFar requires actual mirrored stance");
        const bool mirrored = *host.playback_context.is_mirrored;
        if ((!mirrored && right) || (mirrored && left)) {
            if (!host.physical || !host.physical->foot_frame) return err("MongoPushFootToFar requires actual foot frame");
            return host.physical->foot_frame->out_distance(right)[0] < -0.5f;
        }
        return false;
    }
    return false;
}

}  // namespace s3::game::graph_host::motion_conditions
