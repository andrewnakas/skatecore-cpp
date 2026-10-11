// Port of skate-game/src/graph_host/motion_nodes.rs (out-of-line items).
#include "s3/game/graph_host/motion_nodes.hpp"

#include "s3/core/animation/skeleton_input/name.hpp"

namespace s3::game::graph_host::motion_nodes {

using s3::animation::playback_parameters::ParameterSource;
using s3::animation::playback_parameters::PlaybackParameter;
using s3::animation::skeleton_input::name::encode;

namespace {

/// `str::trim_matches(|c: char| c.is_whitespace() || c == '\0')` (ASCII subset).
std::string trim_name(const std::string& text) {
    auto trimmed = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == '\0';
    };
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && trimmed(text[begin])) ++begin;
    while (end > begin && trimmed(text[end - 1])) --end;
    return text.substr(begin, end - begin);
}

std::string text_or(const Attributes& a, const char* name, const char* fallback) {
    const std::string* v = a.text(name);
    return v ? *v : std::string(fallback);
}

std::optional<std::string> text_owned(const Attributes& a, const char* name) {
    const std::string* v = a.text(name);
    return v ? std::optional<std::string>(*v) : std::nullopt;
}

AttributeName key(const Attributes& a, const char* name, const char* fallback) { return encode(text_or(a, name, fallback)); }

PlayAnimation play(const Attributes& a) {
    PlayAnimation out;
    out.animation = text_or(a, "anim", "");
    out.switch_animation = text_owned(a, "switchAnim");
    out.mirror_animation = text_owned(a, "mirrorAnim");
    out.no_board_animation = text_owned(a, "noBoardAnim");
    out.playback_speed = rs::from_bits(a.float_bits("playBackSpeed", rs::to_bits(1.0f)));
    out.apply_posture = a.boolean_byte("applyPosture", 1) != 0;
    out.transition = transition(a);
    return out;
}

/// The behavior-name dispatch of `MotionFactory::create` (`kind == Behavior`).
rs::Result<std::optional<MotionOperation>, std::string> behavior(const Attributes& a, const std::string& name) {
    using O = MotionOperation;
    using R = rs::Result<std::optional<MotionOperation>, std::string>;
    auto some = [](MotionOperation operation) { return R(std::optional<MotionOperation>(std::move(operation))); };
    {
        auto operation = motion_grind::Operation::parse(a);
        if (operation.is_err()) return rs::make_err(operation.unwrap_err());
        if (operation.unwrap()) return some(O::Grind{*operation.unwrap()});
    }
    if (auto operation = motion_landing::Operation::parse(a)) return some(O::Landing{*operation});
    if (auto operation = motion_hand_services::Operation::parse(a)) return some(O::HandService{*operation});
    if (auto push = PushOperation::from_attributes(a)) return some(O::Push{*push});
    if (auto operation = motion_riding::RidingOperation::parse(a)) return some(O::Riding{*operation});
    {
        auto operation = motion_native::Operation::parse(a);
        if (operation.is_err()) return rs::make_err(operation.unwrap_err());
        if (operation.unwrap()) return some(O::Native{*operation.unwrap()});
    }
    if (auto operation = motion_sliding::Operation::parse(a)) return some(O::Slide{*operation});
    if (auto operation = motion_kickturn::Operation::parse(a)) return some(O::KickTurn{*operation});
    {
        auto operation = motion_shove::ShoveOperation::parse(a);
        if (operation.is_err()) return rs::make_err(operation.unwrap_err());
        if (operation.unwrap()) return some(O::Shove{*operation.unwrap()});
    }
    if (name == "SetDeckPitchAndYaw") {
        // Native constructor82BC7B20 retains authored FastStrings.
        return some(O::SetDeckPitchAndYaw{key(a, "skateyaw", "skateyaw"), key(a, "skatepitch", "skatepitch")});
    }
    if (name == "ResetSkaterAnimation" || name == "ResetToGivenStance") {
        auto operation = motion_reset::parse_operation(a);
        if (!operation) return rs::make_err(std::string("Invalid reset operation"));
        return some(O::ResetAnimation{*operation});
    }
    if (name == "MatchTwistAndLean") return some(O::TwistLean{motion_twist_lean::Operation::parse(a)});
    if (name == "Wipeout") return some(O::Wipeout{motion_wipeout::Operation::Wipeout});
    if (name == "EnableWipeoutGestures") return some(O::Wipeout{motion_wipeout::Operation::EnableGestures});
    if (name == "FilterMotionGraphIntent") return some(O::IntentFilter{motion_intent_filter::Operation::parse(a)});
    if (name == "ToggleBoard") return some(O::ToggleBoard{});
    if (name == "AddRunoutAttribs") {
        if (auto operation = motion_runout::Operation::parse(a)) return some(O::Runout{*operation});
        return std::optional<MotionOperation>();
    }
    if (name == "PlayAnimation") return some(O::Play{play(a)});
    if (name == "PrintText2D") return some(O::PrintText2D{});
    if (name == "ApplyingBodyTilt") return some(O::ApplyingBodyTilt{});
    if (name == "SettingBodyTilt") return some(O::SettingBodyTilt{key(a, "tilt_x", "tilt_x")});
    if (name == "Crouching") return some(O::Crouching{key(a, "crouchName", "")});
    if (name == "Pumping") return some(O::Pumping{});
    if (name == "BipedCadence") return some(O::BipedCadence{});
    if (name == "MatchCadence") return some(O::MatchCadence{});
    if (name == "OffboardBodyTweakBlend") {
        return some(O::OffboardBodyTweakBlend{motion::body_tweak::Settings::parse(a)});
    }
    if (name == "BodySpin") return some(O::BodySpin{});
    if (name == "ControlAirLegExtension") return some(O::AirLeg{motion_air_leg::Operation::parse(a)});
    if (name == "ClearTrickAttr") return some(O::ClearTrickAttr{});
    if (name == "SetTrickHeight" || name == "FootPlantAbsorb" || name == "SetHandPlantAnticLength" ||
        name == "ScoringHandPlants" || name == "SetTrickAttr" || name == "MonitorUnderflip" || name == "SetDark" ||
        name == "UpdateIsWeightOnNose") {
        if (auto operation = motion_tricks::Operation::parse(a)) return some(O::Trick{std::move(*operation)});
        return std::optional<MotionOperation>();
    }
    if (name == "ScoringTrick") {
        auto operation = motion_scoring_trick::Operation::parse(a);
        if (operation.is_err()) return rs::make_err(operation.unwrap_err());
        return some(O::ScoringTrick{operation.unwrap()});
    }
    if (name == "SetBumpCoefficients") return some(O::SetBumpCoefficients{key(a, "X", "BumpX"), key(a, "Y", "BumpY")});
    if (name == "CharacterGesture") return some(O::CharacterGesture{});
    if (name == "EndGesture") return some(O::EndGesture{});
    if (name == "DisallowPumping") return some(O::DisallowPumping{});
    if (name == "FakieHeadChannel") return some(O::FakieHeadChannel{});
    if (name == "SetSpeed") {
        std::optional<float> value;
        if (const auto* v = a.get("setToValue")) value = rs::from_bits(v->float_bits);
        return some(O::SetSpeed{key(a, "attribute", ""), value});
    }
    if (name == "UpdateRidingFakie") {
        s3::animation::riding_fakie::Settings settings;
        settings.high_speed = rs::from_bits(a.float_bits("highSpeedThreshold", rs::to_bits(1.0f)));
        settings.low_speed = rs::from_bits(a.float_bits("lowSpeedThreshold", rs::to_bits(0.5f)));
        settings.slowly_backwards_seconds =
            rs::from_bits(a.float_bits("timeSlowlyRollingBackwardsThreshold", 0x3e4ccccdu));
        settings.after_teleport_seconds = rs::from_bits(a.float_bits("timeFromTeleportThreshold", rs::to_bits(3.0f)));
        return some(O::UpdateRidingFakie{settings});
    }
    if (motion_stock_gameplay::Operation::recognizes(name)) {
        return some(O::StockGameplay{motion_stock_gameplay::Operation::parse(a)});
    }
    // Factory82BC6D40 defaults both identifiers to the string0.
    if (name == "AttachIntent") {
        return some(O::AttachIntent{text_or(a, "intent", "0"), key(a, "attr", "0"), a.boolean_byte("set", 0) != 0});
    }
    // Ctor82BB36D8; layout88 is holding and108 is speed, but
    // Update emits speed before holding.
    if (name == "SetTurning") {
        return some(O::SetTurning{{
            key(a, "angleName", "angle"),
            key(a, "dirName", "dir"),
            key(a, "quicknessName", "quickness"),
            key(a, "speedName", "speedtuck"),
            key(a, "holdingName", "holding"),
        }});
    }
    return std::optional<MotionOperation>();
}

}  // namespace

s3::data::Result<std::optional<MotionOperation>> MotionFactory::create(OperationKind kind, Node /*parent*/,
                                                                       const Attributes& a) {
    const std::string* raw_name = a.text("name");
    if (!raw_name) return s3::data::Error{"MotionGraph operation has no name"};
    const std::string name = trim_name(*raw_name);
    std::optional<MotionOperation> operation;
    if (kind == OperationKind::Condition) {
        auto condition = MotionCondition::parse(a);
        if (condition.is_err()) return s3::data::Error{condition.unwrap_err()};
        if (condition.unwrap()) operation = MotionOperation(MotionOperation::Condition{*condition.unwrap()});
    } else if (kind == OperationKind::Behavior) {
        auto parsed = behavior(a, name);
        if (parsed.is_err()) return s3::data::Error{parsed.unwrap_err()};
        operation = std::move(parsed.unwrap());
    } else if (kind == OperationKind::Hook) {
        if (auto hook = motion_hooks::MotionHook::parse(a)) operation = MotionOperation(MotionOperation::Hook{*hook});
    }
    if (!operation) operation = MotionOperation(MotionOperation::Unsupported{kind, name});
    return operation;
}

s3::data::Status MotionFactory::add_parameter(MotionOperation& instance, const Attributes& a) {
    if (auto* play = std::get_if<MotionOperation::Play>(&instance.value)) {
        //82BB56B8: all unrecognized from strings take filteredIntent.
        const std::string from = text_or(a, "from", "");
        std::optional<ParameterSource> source;
        if (from == "intent") {
            source = ParameterSource(ParameterSource::MotionIntent{text_or(a, "intent", "")});
        } else if (from == "lastAnim") {
            source = ParameterSource(ParameterSource::LastAnimation{key(a, "attribute", "")});
        } else {
            source = ParameterSource(ParameterSource::FilteredIntent{text_or(a, "filteredIntent", "")});
        }
        const bool last_animation = std::holds_alternative<ParameterSource::LastAnimation>(source->value);
        std::optional<AttributeName> rename;
        if (const std::string* v = a.text("rename")) {
            const AttributeName encoded = encode(*v);
            if (encoded != encode("")) rename = encoded;
        }
        std::optional<float> default_value;
        if (const auto* v = a.get("defaultValue")) default_value = rs::from_bits(v->float_bits);
        play->_0.parameters.push_back(PlaybackParameter{std::move(*source), rename, default_value,
                                                        !last_animation && a.boolean_byte("normalize", 0) != 0});
    }
    // Other implemented nodes inherit the verified no-op AddParam.
    return s3::data::ok_status();
}

TransitionSettings transition(const Attributes& a) {
    TransitionSettings out;
    const std::string kind = text_or(a, "transType", "");
    out.kind = kind == "play" ? 1u : kind == "sequence" ? 4u : kind == "channelblend" ? 3u : 2u;
    // Source literal82099280, not a fitted visual blend duration.
    out.seconds = rs::from_bits(a.float_bits("time", 0x3e4ccccdu));
    out.under = a.boolean_byte("transitionUnder", 0) != 0 ? 1u : 0u;
    if (a.boolean_byte("blendWithCurrentFrame", 0) != 0) {
        out.matching = 1;
    } else if (a.boolean_byte("blendMatchPhase", 0) != 0) {
        out.matching = 2;
    } else if (a.boolean_byte("blendMatchFrame", 0) != 0) {
        out.matching = 3;
    } else {
        out.matching = 0;
    }
    out.use_channels_from_weights = a.boolean_byte("useChannelFromWeights", 0) != 0;
    return out;
}

}  // namespace s3::game::graph_host::motion_nodes
