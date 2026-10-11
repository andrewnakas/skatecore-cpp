// Port of skate-game/src/graph_host/motion_instances.rs (module `motion::instances`).
#include "s3/game/graph_host/motion/instances.hpp"

namespace s3::game::graph_host::motion::instances {

Instance Instance::new_(const motion_nodes::MotionOperation& operation) {
    using motion_nodes::MotionOperation;
    namespace stock = motion_stock_gameplay;
    const auto& op = operation.value;
    if (std::holds_alternative<MotionOperation::Grind>(op)) return Grind{motion_grind::State{}};
    if (std::holds_alternative<MotionOperation::Trick>(op)) return Trick{0};

    if (std::holds_alternative<MotionOperation::ToggleBoard>(op)) {
        return ToggleBoard{s3::player::offboard::toggle_board::State{}};
    }
    if (std::holds_alternative<MotionOperation::Runout>(op)) return Runout{motion_runout::State{}};
    if (std::holds_alternative<MotionOperation::IntentFilter>(op)) {
        return IntentFilter{s3::animation::intent_filter::State{}};
    }
    if (std::holds_alternative<MotionOperation::Landing>(op)) return Landing{motion_landing::State{}};
    if (std::holds_alternative<MotionOperation::Wipeout>(op)) return Wipeout{motion_wipeout::State{}};
    if (std::holds_alternative<MotionOperation::TwistLean>(op)) {
        return TwistLean{s3::player::offboard::twist_lean::State{}};
    }
    if (std::holds_alternative<MotionOperation::Play>(op)) return Play{s3::animation::playback::PlayAnimationInstance{}};
    if (const auto* push = std::get_if<MotionOperation::Push>(&op)) {
        return Push{pushing::PushInstance::new_(push->_0)};
    }
    if (std::holds_alternative<MotionOperation::SetTurning>(op)) {
        s3::input::set_turning::State state;
        state.elapsed = 0.0f;
        state.smoothed = 0.0f;
        state.mode = 2;
        return Turning{state};
    }
    if (std::holds_alternative<MotionOperation::Crouching>(op)) return Crouching{std::nullopt};
    if (std::holds_alternative<MotionOperation::SettingBodyTilt>(op)) {
        return BodyTilt{s3::animation::body_tilt::State{}};
    }
    if (std::holds_alternative<MotionOperation::UpdateRidingFakie>(op)) {
        return RidingFakie{s3::animation::riding_fakie::State{}};
    }
    if (std::holds_alternative<MotionOperation::Pumping>(op)) {
        return Pumping{s3::animation::pumping_channel::State::default_()};
    }
    if (std::holds_alternative<MotionOperation::BipedCadence>(op)) return Stateless{};
    if (std::holds_alternative<MotionOperation::MatchCadence>(op)) {
        return MatchCadence{motion_offboard_cadence::MatchCadence{}};
    }
    if (std::holds_alternative<MotionOperation::OffboardBodyTweakBlend>(op)) {
        return OffboardBodyTweak{body_tweak::State{}};
    }
    if (const auto* gameplay = std::get_if<MotionOperation::StockGameplay>(&op)) {
        const auto& g = gameplay->_0.value;
        if (std::holds_alternative<stock::Operation::MatchAirTime>(g)) return MatchAirTime{match_air_time::State{}};
    }
    if (std::holds_alternative<MotionOperation::FakieHeadChannel>(op)) {
        return FakieHead{motion_channels::FakieHead{}};
    }
    if (const auto* native = std::get_if<MotionOperation::Native>(&op)) {
        if (std::holds_alternative<motion_native::Operation::StoreLandingData>(native->_0.value)) {
            return LandingData{motion_native::LandingData{}};
        }
    }
    if (const auto* slide = std::get_if<MotionOperation::Slide>(&op)) {
        if (std::holds_alternative<motion_sliding::Operation::Update>(slide->_0.value)) {
            return Sliding{motion_sliding::State{}};
        }
        if (std::holds_alternative<motion_sliding::Operation::Deceleration>(slide->_0.value)) {
            return SlideDeceleration{0.0f};
        }
    }
    if (const auto* kick = std::get_if<MotionOperation::KickTurn>(&op)) {
        if (std::holds_alternative<motion_kickturn::Operation::Steering>(kick->_0.value)) {
            return KickTurn{s3::animation::kickturn::State::new_()};
        }
    }
    if (std::holds_alternative<MotionOperation::BodySpin>(op)) return BodySpin{motion_spin::State{}};
    if (std::holds_alternative<MotionOperation::AirLeg>(op)) {
        return AirLeg{s3::animation::air_leg_extension::State{}};
    }
    if (std::holds_alternative<MotionOperation::CharacterGesture>(op)) {
        return CharacterGesture{motion_character_gesture::CharacterGesture::new_()};
    }
    if (std::holds_alternative<MotionOperation::Shove>(op)) return Shove{motion_shove::ShoveState::new_()};
    if (const auto* gameplay = std::get_if<MotionOperation::StockGameplay>(&op)) {
        const auto& g = gameplay->_0.value;
        if (std::holds_alternative<stock::Operation::SetManualAngle>(g)) {
            return SetManualAngle{s3::animation::manual::State{}};
        }
        if (std::holds_alternative<stock::Operation::HippyJumpAntic>(g)) {
            return HippyJumpAntic{motion_hippy_jump::State{}};
        }
        if (std::holds_alternative<stock::Operation::FingerFlipOut>(g)) {
            return FingerFlipOut{motion_finger_flip::State{}};
        }
        if (std::holds_alternative<stock::Operation::JumpInto>(g)) return JumpInto{true};
    }
    if (std::holds_alternative<MotionOperation::ResetAnimation>(op)) return Stateless{};
    return Stateless{};
}

}  // namespace s3::game::graph_host::motion::instances
