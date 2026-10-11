// Port of skate-game/src/graph_host/motion_execute.rs (module `motion::execution`).
#include "s3/game/graph_host/motion/execution.hpp"

#include <type_traits>
#include <variant>

#include "s3/data/result.hpp"
#include "s3/game/graph_host/action_nodes.hpp"
#include "s3/game/graph_host/motion/leaf_execution.hpp"
#include "s3/game/graph_host/motion_landing_execute.hpp"
#include "s3/game/graph_host/motion_reset.hpp"

namespace s3::game::graph_host::motion {

namespace execution {

namespace {

constexpr const char* OPERATION_NAMES[] = {
    "Grind",          "Trick",          "SetDeckPitchAndYaw",
    "Play",           "PrintText2D",    "ToggleBoard",
    "Runout",         "AttachIntent",   "Push",
    "SetTurning",     "ApplyingBodyTilt", "SettingBodyTilt",
    "Crouching",      "Pumping",        "BipedCadence",
    "MatchCadence",   "OffboardBodyTweakBlend", "DisallowPumping",
    "FakieHeadChannel", "Native",       "Slide",
    "KickTurn",       "BodySpin",       "AirLeg",
    "ClearTrickAttr", "ScoringTrick",   "SetBumpCoefficients",
    "Landing",        "Wipeout",        "TwistLean",
    "HandService",    "IntentFilter",   "CharacterGesture",
    "EndGesture",     "Shove",          "ResetAnimation",
    "UpdateRidingFakie", "StockGameplay", "SetSpeed",
    "Hook",           "Riding",         "Condition",
    "Unsupported",
};
static_assert(sizeof(OPERATION_NAMES) / sizeof(OPERATION_NAMES[0]) == std::variant_size_v<MotionOperation::Value>);

constexpr const char* STOCK_GAMEPLAY_NAMES[] = {
    "MatchAirTime",       "UpdateStandingOnCar", "InitMovingObjects",       "MovingObject",
    "SetGrabType",        "JumpInto",            "ScoringHandPlants",       "SetHandPlantAnticLength",
    "ScoringGrabs",       "AirDismounting",      "TweakProject",            "FootPlantAbsorb",
    "FingerFlipOut",      "EnterSkitchingBehaviour", "SkitchingBehaviour",  "SkitchShimmyingBehaviour",
    "SetManualAngle",     "HippyJumpAntic",      "LandOnBoard",
};
static_assert(sizeof(STOCK_GAMEPLAY_NAMES) / sizeof(STOCK_GAMEPLAY_NAMES[0]) ==
              std::variant_size_v<motion_stock_gameplay::Operation::Value>);

}  // namespace

// Only the variant name (and the complete `Unsupported` / unit-variant forms) is reproduced;
// payload fields of the other variants are elided as `(..)`, which changes diagnostic text only.
std::string debug_operation(const MotionOperation& operation) {
    if (const auto* unsupported = std::get_if<MotionOperation::Unsupported>(&operation.value)) {
        return std::string("Unsupported { kind: ") + action_nodes::debug_kind(unsupported->kind) +
               ", name: " + s3::data::debug_str(unsupported->name) + " }";
    }
    if (const auto* gameplay = std::get_if<MotionOperation::StockGameplay>(&operation.value)) {
        return "StockGameplay(" + debug_stock_gameplay(gameplay->_0) + ")";
    }
    const std::string name = OPERATION_NAMES[operation.value.index()];
    const bool unit = std::visit([](const auto& v) { return std::is_empty_v<std::decay_t<decltype(v)>>; },
                                 operation.value);
    return unit ? name : name + "(..)";
}

std::string debug_stock_gameplay(const motion_stock_gameplay::Operation& operation) {
    const std::string name = STOCK_GAMEPLAY_NAMES[operation.value.index()];
    const bool unit = std::visit([](const auto& v) { return std::is_empty_v<std::decay_t<decltype(v)>>; },
                                 operation.value);
    return unit ? name : name + " { .. }";
}

}  // namespace execution

rs::Result<rs::Unit, std::string> MotionHost::execute(BehaviorId behavior, const Frame& frame, std::uint8_t phase) {
    if (behavior >= remap.behaviors.size()) return rs::make_err(std::string("Unbound MotionGraph behavior"));
    const std::size_t id = remap.behaviors[behavior];
    MotionOperation operation = operations[id];
    if (const auto* grind = std::get_if<MotionOperation::Grind>(&operation.value)) {
        const motion_grind::Operation& grind_operation = grind->_0;
        if (phase == 2 ||
            (phase == 0 && std::holds_alternative<motion_grind::Operation::Attributes>(grind_operation.value))) {
            return rs::Unit{};
        }
        S3_MHOST_SOME(motion_grind::Physical grind_physical_, grind_physical,
                       "Grind graph requires completed physical grind observations");
        if ((phase == 0 && std::holds_alternative<motion_grind::Operation::Fade>(grind_operation.value)) ||
            (phase == 1 && grind_physical_.grinding &&
             std::holds_alternative<motion_grind::Operation::Attributes>(grind_operation.value))) {
            S3_MHOST_SOME(const std::uint32_t animation_flags, animation.skater_animation_flags,
                           "Grind graph requires live animation stance");
            grind_physical_.animation_mirrored = (animation_flags & 0x40000000u) != 0;
        }
        if (behavior >= instances.size()) return rs::make_err(std::string("Unallocated grind behavior"));
        auto* state = instances[behavior].get<Instance::Grind>();
        if (state == nullptr) return rs::make_err(std::string("Grind operation/instance mismatch"));
        return motion_grind::execute(state->_0, grind_operation, phase, frame.dt, grind_settings, grind_physical_,
                                     animation);
    }
    if (const auto* trick = std::get_if<MotionOperation::Trick>(&operation.value)) {
        Instance instance = std::exchange(instances.at(behavior), Instance(Instance::Stateless{}));
        auto* updates = instance.get<Instance::Trick>();
        if (updates == nullptr) return rs::make_err(std::string("Trick operation/instance mismatch"));
        auto result = motion_tricks::execute(*this, trick->_0, updates->_0, phase);
        instances[behavior] = Instance::Trick{updates->_0};
        return result;
    }
    if (operation.is<MotionOperation::ToggleBoard>()) return execute_toggle_board(behavior, phase);
    if (operation.is<MotionOperation::Runout>()) {
        // The native producer samples a physical runout bundle at Begin.
        // This host does not yet publish that bundle, so retain the
        // source-backed state without inventing angle/speed values.
        if (behavior >= instances.size()) return rs::make_err(std::string("Unallocated runout behavior"));
        auto* state = instances[behavior].get<Instance::Runout>();
        if (state == nullptr) return rs::make_err(std::string("Runout operation/instance mismatch"));
        if (phase == 0) {
            state->_0.begin(runout_physical);
            return rs::Unit{};
        }
        auto result = state->_0.update(&animation);
        if (result.is_err()) return rs::make_err(std::string(result.unwrap_err()));
        return rs::Unit{};
    }
    S3_MHOST_TRY(const bool score_or_bump, execute_score_or_bump(operation, phase));
    if (score_or_bump) return rs::Unit{};
    if (auto* slide = std::get_if<MotionOperation::Slide>(&operation.value)) {
        return execute_slide(behavior, std::move(slide->_0), frame, phase);
    }
    if (operation.is<MotionOperation::ClearTrickAttr>() || operation.is<MotionOperation::AirLeg>() ||
        operation.is<MotionOperation::BodySpin>()) {
        return execute_air(behavior, std::move(operation), frame, phase);
    }
    if (auto* twist_lean = std::get_if<MotionOperation::TwistLean>(&operation.value)) {
        return execute_twist_lean(behavior, std::move(twist_lean->_0), phase);
    }
    if (auto* wipeout = std::get_if<MotionOperation::Wipeout>(&operation.value)) {
        return execute_wipeout(behavior, std::move(wipeout->_0), phase);
    }
    if (auto* filter = std::get_if<MotionOperation::IntentFilter>(&operation.value)) {
        return execute_intent_filter(behavior, std::move(filter->_0), frame, phase);
    }
    if (const auto* reset = std::get_if<MotionOperation::ResetAnimation>(&operation.value)) {
        if (phase == 0) {
            if (reset->_0 == motion_reset::Operation::SkaterAnimation) {
                action_intents.clear();
                action_controls = ActionControls{};
                animation.motion_intents.clear();
                animation.filtered_intents.clear();
                //825953B0's represented selective MG output reset.
                flags = motion_landing::Flags{};
                is_power_sliding = false;
                riding.last_good_landing_velocity = 0.0f;
                riding.manual_out_timer = 0.0f;
                hand_services.busy_hands = {0, 0};
                gesture_publication = std::nullopt;
                animation.reset_from_stock();
            } else {
                S3_MHOST_CHECK(animation.reset_to_given_stance());
            }
            //Later PlayAnimation nodes in this same traversal see the reset.
            playback_context.is_mirrored =
                animation.skater_animation_flags
                    ? std::optional<bool>((*animation.skater_animation_flags & 0x40000000u) != 0)
                    : std::nullopt;
            playback_context.is_switch = animation.relative_stance == 1;
        }
        return rs::Unit{};
    }
    if (const auto* hand = std::get_if<MotionOperation::HandService>(&operation.value)) {
        hand_services.execute(hand->_0, phase, animation);
        return rs::Unit{};
    }
    if (const auto* landing = std::get_if<MotionOperation::Landing>(&operation.value)) {
        if (behavior >= instances.size()) return rs::make_err(std::string("Unallocated landing behavior"));
        auto* state = instances[behavior].get<Instance::Landing>();
        if (state == nullptr) return rs::make_err(std::string("Landing operation/instance mismatch"));
        //StoreLandingData can update C54 earlier in this same graph traversal.
        std::optional<motion_landing::Physical> landing_physical_ = landing_physical;
        if (landing_physical_) landing_physical_->last_good_landing_velocity = riding.last_good_landing_velocity;
        return motion_landing_execute::execute(landing->_0, state->_0, flags, animation, condition_random,
                                               landing_physical_, playback_context.is_mirrored, frame.dt, phase);
    }
    return leaf_execution::execute(*this, behavior, std::move(operation), frame, phase);
}

}  // namespace s3::game::graph_host::motion
