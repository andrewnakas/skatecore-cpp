// Port of skate-game/src/graph_host/motion_instances.rs (module `motion::instances`).
//! Per-node allocation retained by the production MotionHost.
#pragma once
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include "s3/core/animation/air_leg_extension.hpp"
#include "s3/core/animation/body_tilt.hpp"
#include "s3/core/animation/crouching.hpp"
#include "s3/core/animation/intent_filter.hpp"
#include "s3/core/animation/kickturn.hpp"
#include "s3/core/animation/manual.hpp"
#include "s3/core/animation/playback.hpp"
#include "s3/core/animation/pumping_channel.hpp"
#include "s3/core/animation/riding_fakie.hpp"
#include "s3/core/input/set_turning.hpp"
#include "s3/core/player/offboard/toggle_board.hpp"
#include "s3/core/player/offboard/twist_lean.hpp"
#include "s3/game/graph_host/motion/body_tweak.hpp"
#include "s3/game/graph_host/motion/match_air_time.hpp"
#include "s3/game/graph_host/motion_channels.hpp"
#include "s3/game/graph_host/motion_character_gesture.hpp"
#include "s3/game/graph_host/motion_finger_flip.hpp"
#include "s3/game/graph_host/motion_grind.hpp"
#include "s3/game/graph_host/motion_hippy_jump.hpp"
#include "s3/game/graph_host/motion_landing.hpp"
#include "s3/game/graph_host/motion_native.hpp"
#include "s3/game/graph_host/motion_nodes.hpp"
#include "s3/game/graph_host/motion_offboard_cadence.hpp"
#include "s3/game/graph_host/motion_runout.hpp"
#include "s3/game/graph_host/motion_shove.hpp"
#include "s3/game/graph_host/motion_sliding.hpp"
#include "s3/game/graph_host/motion_spin.hpp"
#include "s3/game/graph_host/motion_wipeout.hpp"
#include "s3/game/graph_host/pushing.hpp"

namespace s3::game::graph_host::motion::instances {

/// `pub(super)` in Rust. Rust `enum Instance`; each variant is a nested struct held in `value`.
struct Instance {
    struct Grind {
        motion_grind::State _0;
    };
    struct Trick {
        std::int32_t _0 = 0;
    };
    struct Stateless {};
    struct MatchCadence {
        motion_offboard_cadence::MatchCadence _0;
    };
    struct ToggleBoard {
        s3::player::offboard::toggle_board::State _0;
    };
    struct Runout {
        motion_runout::State _0;
    };
    struct IntentFilter {
        s3::animation::intent_filter::State _0;
    };
    struct Landing {
        motion_landing::State _0;
    };
    struct Wipeout {
        motion_wipeout::State _0;
    };
    struct OffboardBodyTweak {
        body_tweak::State _0;
    };
    struct MatchAirTime {
        match_air_time::State _0;
    };
    struct TwistLean {
        s3::player::offboard::twist_lean::State _0;
    };
    struct Play {
        s3::animation::playback::PlayAnimationInstance _0;
    };
    struct Push {
        pushing::PushInstance _0;
    };
    struct Turning {
        s3::input::set_turning::State _0;
    };
    struct Crouching {
        std::optional<s3::animation::crouching::State> _0;
    };
    struct BodyTilt {
        s3::animation::body_tilt::State _0;
    };
    struct RidingFakie {
        s3::animation::riding_fakie::State _0;
    };
    struct Pumping {
        s3::animation::pumping_channel::State _0;
    };
    struct FakieHead {
        motion_channels::FakieHead _0;
    };
    struct LandingData {
        motion_native::LandingData _0;
    };
    struct Sliding {
        motion_sliding::State _0;
    };
    struct SlideDeceleration {
        float _0 = 0.0f;
    };
    struct KickTurn {
        s3::animation::kickturn::State _0;
    };
    struct BodySpin {
        motion_spin::State _0;
    };
    struct AirLeg {
        s3::animation::air_leg_extension::State _0;
    };
    struct CharacterGesture {
        motion_character_gesture::CharacterGesture _0;
    };
    struct Shove {
        motion_shove::ShoveState _0;
    };
    struct SetManualAngle {
        s3::animation::manual::State _0;
    };
    struct HippyJumpAntic {
        motion_hippy_jump::State _0;
    };
    struct FingerFlipOut {
        motion_finger_flip::State _0;
    };
    ///82BB5930 initializes byte8 once per allocated behavior instance.
    struct JumpInto {
        bool first_update = false;
    };

    using Value = std::variant<Grind, Trick, Stateless, MatchCadence, ToggleBoard, Runout, IntentFilter, Landing,
                               Wipeout, OffboardBodyTweak, MatchAirTime, TwistLean, Play, Push, Turning, Crouching,
                               BodyTilt, RidingFakie, Pumping, FakieHead, LandingData, Sliding, SlideDeceleration,
                               KickTurn, BodySpin, AirLeg, CharacterGesture, Shove, SetManualAngle, HippyJumpAntic,
                               FingerFlipOut, JumpInto>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    Instance(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }
    /// `if let Instance::V(x) = instance`: nullptr on a different variant.
    template <class V>
    V* get() {
        return std::get_if<V>(&value);
    }
    template <class V>
    const V* get() const {
        return std::get_if<V>(&value);
    }

    /// `pub(super)` in Rust.
    static Instance new_(const motion_nodes::MotionOperation& operation);
};

}  // namespace s3::game::graph_host::motion::instances
