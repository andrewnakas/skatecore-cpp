// Port of skate-game/src/graph_host/motion_execute_leaf.rs (module `motion::leaf_execution`).
#include "s3/game/graph_host/motion/leaf_execution.hpp"

#include <cmath>

#include "s3/core/animation/channel_playback.hpp"
#include "s3/core/animation/playback_parameters.hpp"
#include "s3/core/animation/skeleton_input/name.hpp"
#include "s3/game/graph_host/action_nodes.hpp"
#include "s3/game/graph_host/motion/execution.hpp"
#include "s3/game/graph_host/motion_kickturn.hpp"
#include "s3/game/graph_host/motion_offboard_cadence.hpp"

namespace s3::game::graph_host::motion::leaf_execution {

namespace {

using s3::animation::playback_parameters::SettableAttribute;
using s3::animation::skeleton_input::name::encode;

SettableAttribute attribute(s3::animation::output::attributes::AttributeName name, float value) {
    return SettableAttribute{name, value, false, -1};
}

rs::Result<rs::Unit, std::string> execute_stock_gameplay(MotionHost& host, Instance& instance,
                                                         const motion_stock_gameplay::Operation& operation,
                                                         const Frame& frame, std::uint8_t phase) {
    // These are graph-owned lifecycle records. Their live values
    // are published through the same host state consumed by the
    // matching conditions; the authored node itself must still
    // advance that lifecycle instead of aborting graph execution.
    using Op = motion_stock_gameplay::Operation;
    if (const auto* finger = std::get_if<Op::FingerFlipOut>(&operation.value)) {
        auto* state = instance.get<Instance::FingerFlipOut>();
        if (state == nullptr) return rs::make_err(std::string("FingerFlipOut operation/instance mismatch"));
        if (phase == 0) {
            state->_0.begin();
        } else if (phase == 1) {
            const float value = state->_0.update(host.animation.motion_intents.contains_key(finger->grab_intent),
                                                 frame.dt, host.finger_flip);
            host.animation.set_attribute(attribute(encode("Grabbing"), value));
        }
    } else if (operation.is<Op::MatchAirTime>()) {
        auto* state = instance.get<Instance::MatchAirTime>();
        if (state == nullptr) return rs::make_err(std::string("MatchAirTime operation/instance mismatch"));
        if (phase != 2) {
            S3_MHOST_SOME(const auto p, host.gameplay_conditions, "MatchAirTime requires completed OffBoard output");
            if (phase == 0) {
                S3_MHOST_SOME(const float cadence, host.offboard_cadence_phase,
                               "MatchAirTime requires completed OffBoard80");
                state->_0.begin(cadence, p.offboard_air_scalar_92, p.offboard_air_translation);
            } else {
                state->_0.update(host.animation, p.offboard_time_to_land, p.offboard_air_scalar_92,
                                 p.offboard_air_translation);
            }
        }
    } else if (const auto* init = std::get_if<Op::InitMovingObjects>(&operation.value)) {
        if (phase == 0) host.moving_objects.register_(init->path);
    } else if (const auto* moving = std::get_if<Op::MovingObject>(&operation.value)) {
        switch (phase) {
            case 0: S3_MHOST_CHECK(host.moving_objects.begin(moving->path)); break;
            case 1: {
                S3_MHOST_TRY(const bool active, host.moving_objects.update(moving->path));
                if (!active) return rs::make_err("MovingObject is not active: " + moving->path);
                break;
            }
            default: host.moving_objects.end(); break;
        }
    } else if (operation.is<Op::LandOnBoard>()) {
        //82BB95D8 -> FlipBoardBackwards82B97150. End/Update are empty.
        if (phase == 0) {
            if (!host.gameplay_conditions) {
                return rs::make_err(std::string("LandOnBoard requires completed physical output"));
            }
            if (host.gameplay_conditions->landing_turning) {
                if (!host.animation.skater_animation_flags) {
                    return rs::make_err(std::string("LandOnBoard requires SkaterAnim orientation"));
                }
                *host.animation.skater_animation_flags ^= 0x80000000u;
            }
        }
    } else if (operation.is<Op::UpdateStandingOnCar>()) {
        // This node is a consumer of the moving-object
        // publication.  It must not turn an absent publication
        // into a fatal graph error: the native dispatcher keeps
        // the last valid physical owner state and continues the
        // frame.  A moving-object owner can publish its update
        // through the registry when one is actually active.
    } else if (const auto* grab_type = std::get_if<Op::SetGrabType>(&operation.value)) {
        S3_MHOST_TRY(const auto grab, motion_stock_gameplay::grab_type_parse(grab_type->grab));
        if (phase == 0) {
            host.animation.set_grab_type(grab);
        } else if (phase == 2) {
            host.animation.clear_grab_type();
        }
    } else if (const auto* grabs = std::get_if<Op::ScoringGrabs>(&operation.value)) {
        if (phase == 1) {
            // TU3 82BBEF60 publishes a score name and vector.
            // It never starts animations or changes channel influence.
            const float first = host.animation.filtered_intent(grabs->intent_x).value_or(0.0f);
            const float second = host.animation.filtered_intent(grabs->intent_y).value_or(0.0f);
            float x;
            float y;
            if (grabs->polar) {
                x = second * rs::sin(first);
                const float yy = second * rs::cos(first);
                y = grabs->invert_y ? -yy : yy;
            } else {
                x = first;
                y = second;
            }
            auto as_view = [](const std::optional<std::string>& value) {
                return value ? std::optional<std::string_view>(*value) : std::nullopt;
            };
            const std::string_view selected = motion_stock_gameplay::select_grab_score(
                grabs->grab_name, {as_view(grabs->up), as_view(grabs->left), as_view(grabs->down), as_view(grabs->right)},
                x, y);
            host.score_packet.grab = std::make_pair(encode(selected), std::array<float, 2>{x, y});
        }
    } else if (const auto* tweak = std::get_if<Op::TweakProject>(&operation.value)) {
        if (phase == 1) {
            // TU3 82BABFD0/82BABFFC read the same slot+8 map
            // written by FilterMotionGraphIntent at 82BB19D0.
            // This is filtered output, NOT raw AG-produced MG
            // input. Bypassing it removes authored tweak timing.
            const std::optional<float> source_x = host.animation.filtered_intent(tweak->intent_x);
            const std::optional<float> source_y = host.animation.filtered_intent(tweak->intent_y);
            if (!source_x && !source_y) return rs::Unit{};
            float x = source_x.value_or(0.0f);
            float y = source_y.value_or(0.0f);
            const float total = rs::abs(x) + rs::abs(y);
            if (total > 1.0f) {
                const float scale = 1.0f / total;
                x *= scale;
                y *= scale;
            }
            host.animation.set_attribute(attribute(tweak->attribute_x, x));
            host.animation.set_attribute(attribute(tweak->attribute_y, y));
        }
    } else if (const auto* jump = std::get_if<Op::JumpInto>(&operation.value)) {
        auto* state = instance.get<Instance::JumpInto>();
        if (state == nullptr) return rs::make_err(std::string("JumpInto instance was not allocated"));
        //8231FBDC Begin/End are empty. Update82BACDC0 consumes
        //the instance latch after the query, including a miss.
        if (phase == 1 && state->first_update) {
            S3_MHOST_CHECK(host.animation.jump_into(jump->attribute));
            state->first_update = false;
        }
    } else if (const auto* manual = std::get_if<Op::SetManualAngle>(&operation.value)) {
        auto* state = instance.get<Instance::SetManualAngle>();
        if (state == nullptr) return rs::make_err(std::string("SetManualAngle instance was not allocated"));
        switch (phase) {
            case 0: state->_0.begin(); break;
            case 1: {
                const float value = state->_0.update(host.animation.motion_intent("Manual"), host.manual);
                host.animation.set_attribute(attribute(manual->attribute, value));
                break;
            }
            // Vtable8231F88C End points to empty82B61BB8.
            default: break;
        }
    } else if (operation.is<Op::HippyJumpAntic>()) {
        auto* state = instance.get<Instance::HippyJumpAntic>();
        if (state == nullptr) return rs::make_err(std::string("HippyJumpAntic instance was not allocated"));
        switch (phase) {
            case 0: state->_0.begin(); break;
            case 1: state->_0.update(host.animation, host.hippy_jump); break;
            case 2: state->_0.end(); break;
            default: S3_PANIC("internal error: entered unreachable code");
        }
    } else {
        return rs::make_err("MotionGraph stock gameplay producer " + execution::debug_stock_gameplay(operation) +
                            " is not implemented");
    }
    return rs::Unit{};
}

}  // namespace

rs::Result<rs::Unit, std::string> execute(MotionHost& host, BehaviorId behavior, MotionOperation operation,
                                          const Frame& frame, std::uint8_t phase) {
    if (behavior >= host.instances.size()) return rs::make_err(std::string("Unallocated MotionGraph behavior"));
    Instance& instance = host.instances[behavior];
    auto invalid = [&]() -> rs::Result<rs::Unit, std::string> {
        return rs::make_err("Invalid MotionGraph behavior instance " + execution::debug_operation(operation));
    };
    const auto& op = operation.value;

    if (operation.is<MotionOperation::BipedCadence>()) {
        if (phase == 1) {
            motion_offboard_cadence::biped_cadence(host.offboard_cadence_phase, &host.animation_phase, phase);
        }
    } else if (operation.is<MotionOperation::MatchCadence>()) {
        auto* state = instance.get<Instance::MatchCadence>();
        if (state == nullptr) return invalid();
        switch (phase) {
            case 0: state->_0.begin(host.offboard_cadence_phase, true); break;
            case 1: state->_0.update(&host.animation); break;
            default: state->_0.end(); break;
        }
    } else if (const auto* tweak = std::get_if<MotionOperation::OffboardBodyTweakBlend>(&op)) {
        auto* state = instance.get<Instance::OffboardBodyTweak>();
        if (state == nullptr) return invalid();
        switch (phase) {
            case 0: state->_0.begin(); break;
            case 1: {
                if (!host.gameplay_conditions) {
                    return rs::make_err(std::string("OffboardBodyTweakBlend requires completed OffBoard output"));
                }
                const auto& physical = *host.gameplay_conditions;
                if (!host.playback_context.board_available) {
                    return rs::make_err(std::string("OffboardBodyTweakBlend requires OffBoard311"));
                }
                body_tweak::Physical tweak_physical{};
                tweak_physical.time_to_land = physical.offboard_time_to_land;
                tweak_physical.scalar_92 = physical.offboard_air_scalar_92;
                tweak_physical.board_held = *host.playback_context.board_available;
                S3_MHOST_CHECK(
                    state->_0.update(tweak->_0, host.animation, host.wipeout_controls, tweak_physical));
                break;
            }
            default: state->_0.end(host.animation); break;
        }
    } else if (const auto* deck = std::get_if<MotionOperation::SetDeckPitchAndYaw>(&op)) {
        // Begin82BAF100 copies completed Skeleton536 then540.
        // Vtable8231FF24 Update/End both point to empty82B61BB8.
        if (phase == 0) {
            S3_MHOST_SOME(const auto values, host.deck_yaw_pitch,
                           "SetDeckPitchAndYaw requires completed Skeleton output");
            host.animation.set_attribute(attribute(deck->yaw, values[0]));
            host.animation.set_attribute(attribute(deck->pitch, values[1]));
        }
    } else if (operation.is<MotionOperation::PrintText2D>()) {
        //Retail ctor82BA58E8 retains only diagnostic text/layout. All three
        //lifecycle slots in82309664 are82B61BB8 (blr), with no state writes.
    } else if (const auto* shove = std::get_if<MotionOperation::Shove>(&op)) {
        auto* state = instance.get<Instance::Shove>();
        if (state == nullptr) return invalid();
        switch (phase) {
            case 0: state->_0.begin(); break;
            case 1: {
                S3_MHOST_SOME(const auto shove_physical, host.shove_physical,
                               "Shove requires actual interaction output");
                S3_MHOST_SOME(const bool mirrored, host.playback_context.is_mirrored,
                               "Shove requires animation stance");
                S3_MHOST_CHECK(state->_0.update(shove->_0, host.animation, shove_physical,
                                                 host.hand_services.busy_hands, mirrored));
                break;
            }
            default: state->_0.end(host.animation, host.hand_services.keep_shove_channels); break;
        }
    } else if (operation.is<MotionOperation::CharacterGesture>()) {
        auto* state = instance.get<Instance::CharacterGesture>();
        if (state == nullptr) return invalid();
        if (phase == 1) {
            S3_MHOST_SOME(const auto physical, host.gesture_physical,
                           "CharacterGesture requires original physical and hostgesture publication");
            const IntentMap gesture_intents = host.animation.motion_intents;
            if (!host.condition_inputs.physical_state) {
                return rs::make_err(std::string("CharacterGesture requires filteredcategory"));
            }
            if (!host.playback_context.board_available) {
                return rs::make_err(std::string("CharacterGesture requires boardheldbyte311"));
            }
            if (!host.crouching_physical) return rs::make_err(std::string("CharacterGesture requires actualheight"));
            const motion_character_gesture::CharacterGestureInputs inputs{
                gesture_intents,
                host.hand_services.busy_hands,
                host.condition_inputs.physical_state->category,
                physical.ground321,
                *host.playback_context.board_available,
                physical.state_offboard75,
                host.crouching_physical->animation_height_72,
                physical.selections,
                physical.suppress_up,
                physical.force_brake_bypass,
            };
            S3_MHOST_TRY(auto output, state->_0.update(host.animation, inputs));
            if (output) host.gesture_publication = *output;
        }
    } else if (operation.is<MotionOperation::EndGesture>()) {
        if (phase == 0) {
            // EndGesture has no independent instance; it tears down
            // the persistent CharacterGesture owner.
            host.end_gesture_channels();
        }
    } else if (const auto* native = std::get_if<MotionOperation::Native>(&op)) {
        const auto& native_op = native->_0.value;
        if (std::holds_alternative<motion_native::Operation::StoreLandingData>(native_op)) {
            auto* state = instance.get<Instance::LandingData>();
            if (state == nullptr) return invalid();
            if (phase == 1) {
                if (!host.condition_inputs.physical_state) {
                    return rs::make_err(std::string("StoreLandingData requires physical category"));
                }
                S3_MHOST_SOME(const auto native_physical, host.native_physical,
                               "StoreLandingData requires raw physical COM velocity/up");
                state->_0.update(host.condition_inputs.physical_state->category, native_physical, host.riding);
            }
        } else if (const auto* shimmy = std::get_if<motion_native::Operation::EndShimmy>(&native_op)) {
            if (phase == 0) motion_native::end_shimmy(host.animation, shimmy->_0);
        } else if (const auto* score = std::get_if<motion_native::Operation::Score>(&native_op)) {
            if (phase == 1) {
                S3_MHOST_SOME(const bool mirrored, host.playback_context.is_mirrored,
                               "Score augmentation requires animation stance");
                host.score_packet.set(mirrored ? score->mirrored : score->regular);
            }
        } else {
            return invalid();
        }
    } else if (const auto* kick = std::get_if<MotionOperation::KickTurn>(&op)) {
        if (std::holds_alternative<motion_kickturn::Operation::ResetTimer>(kick->_0.value)) {
            //82BAE8D0 is both Begin and End; v136=8258F930 clears3224.
            if (phase == 0 || phase == 2) host.riding.time_since_kickturn = 0.0f;
        } else if (const auto* steering = std::get_if<motion_kickturn::Operation::Steering>(&kick->_0.value)) {
            auto* state = instance.get<Instance::KickTurn>();
            if (state == nullptr) return invalid();
            S3_MHOST_CHECK(
                motion_kickturn::execute(state->_0, steering->_0, host.kickturn, host.animation, frame.dt, phase));
        } else {
            return invalid();
        }
    } else if (operation.is<MotionOperation::FakieHeadChannel>()) {
        auto* state = instance.get<Instance::FakieHead>();
        if (state == nullptr) return invalid();
        if (phase == 1) {
            S3_MHOST_SOME(const std::uint32_t animation_flags, host.animation.skater_animation_flags,
                           "FakieHeadChannel requires actual animation flags");
            const bool fakie = (animation_flags & 0x20000000u) != 0;
            S3_MHOST_CHECK(state->_0.update(host.animation, host.flags.manualing, host.is_power_sliding, fakie));
        }
    } else if (operation.is<MotionOperation::Pumping>()) {
        auto* state = instance.get<Instance::Pumping>();
        if (state == nullptr) return invalid();
        static const char* const NAMES[5] = {"PUMP0", "PUMP1", "PUMP2", "PUMP3", "PUMP4"};  //82F883C8
        if (phase == 2) {
            for (const char* name : NAMES) host.animation.channels.end(name);
        } else if (phase == 1 && host.allow_pumping) {
            const std::array<bool, 5> occupied =
                rs::from_fn<5>([&](std::size_t i) { return host.animation.channels.has(NAMES[i]); });
            S3_MHOST_SOME(const float acceleration, host.pumping_acceleration,
                           "Pumping requires actual ground pumping acceleration");
            const auto update = state->_0.update(acceleration, occupied, host.pumping);
            if (update.start) {
                s3::animation::channel_playback::ChannelSettings settings{};
                settings.priority = 0;
                settings.keep_alive = false;
                settings.mirrored = false;
                settings.speed = 1.0f;
                settings.blend_in = host.pumping.blend_in;
                settings.hold_during_blend_in = false;
                settings.blend_out = host.pumping.blend_out;
                settings.hold_during_blend_out = true;
                settings.use_attributes = false;
                S3_MHOST_CHECK(host.animation.new_channel(NAMES[*update.start], "B_PUMP", settings));
            }
            if (update.influence) host.animation.channels.influence(NAMES[update.influence->first], update.influence->second);
        }
    } else if (operation.is<MotionOperation::DisallowPumping>()) {
        if (phase == 0) {
            for (const char* name : {"PUMP0", "PUMP1", "PUMP2", "PUMP3", "PUMP4"}) {
                host.animation.channels.end_with(name, rs::from_bits(0x3dcccccd), false);
            }
            host.allow_pumping = false;
        } else if (phase == 2) {
            host.allow_pumping = true;
        }
    } else if (const auto* speed = std::get_if<MotionOperation::SetSpeed>(&op)) {
        if (phase == 1) {
            //82BB0948 sets a tree parameter; it does not call SetSpeed
            //on the animation clock. Source uses abs(PhysOutMotion164).
            float value;
            if (speed->value) {
                value = *speed->value;
            } else {
                if (!host.crouching_physical) {
                    return rs::make_err(std::string("SetSpeed requires actual ground-projected speed"));
                }
                value = host.crouching_physical->body_164;
            }
            host.animation.set_attribute(attribute(speed->name, rs::abs(value)));
        }
    } else if (const auto* fakie_settings = std::get_if<MotionOperation::UpdateRidingFakie>(&op)) {
        auto* state = instance.get<Instance::RidingFakie>();
        if (state == nullptr) return invalid();
        if (phase == 1) {
            S3_MHOST_SOME(auto physical, host.fakie_physical, "UpdateRidingFakie requires actual physical outputs");
            physical.doing_trick = host.flags.doing_trick;
            if (const std::optional<bool> fakie = state->_0.update(physical, frame.dt, fakie_settings->_0)) {
                if (!host.animation.skater_animation_flags) {
                    return rs::make_err(std::string("UpdateRidingFakie requires actual animation flags"));
                }
                std::uint32_t& flags = *host.animation.skater_animation_flags;
                flags = (flags & ~0x20000000u) | (*fakie ? 0x20000000u : 0u);
            }
        }
    } else if (const auto* gameplay = std::get_if<MotionOperation::StockGameplay>(&op)) {
        return execute_stock_gameplay(host, instance, gameplay->_0, frame, phase);
    } else if (const auto* tilt = std::get_if<MotionOperation::SettingBodyTilt>(&op)) {
        auto* state = instance.get<Instance::BodyTilt>();
        if (state == nullptr) return invalid();
        if (phase == 1 && host.applying_body_tilt) {
            S3_MHOST_SOME(const bool mirrored, host.playback_context.is_mirrored,
                           "SettingBodyTilt needs actual mirrored stance");
            S3_MHOST_SOME(const auto tilt_physical, host.body_tilt_physical,
                           "SettingBodyTilt needs actual PhysOut tilt and spin");
            if (const std::optional<float> value = state->_0.update(true, mirrored, tilt_physical, host.body_tilt)) {
                host.animation.set_attribute(attribute(tilt->_0, *value));
            }
        } else if (phase == 1) {
            // Disabled native branch updates the enable latch without
            // reading physics or emitting a parameter.
            state->_0.disable();
        }
    } else if (const auto* play = std::get_if<MotionOperation::Play>(&op)) {
        auto* state = instance.get<Instance::Play>();
        if (state == nullptr) return invalid();
        switch (phase) {
            case 0: S3_MHOST_CHECK(state->_0.begin(play->_0, host.playback_context, host.animation)); break;
            case 1: S3_MHOST_CHECK(state->_0.update(play->_0, host.animation)); break;
            default: state->_0.end(); break;
        }
    } else if (const auto* attach = std::get_if<MotionOperation::AttachIntent>(&op)) {
        if (phase == 1) host.animation.attach(attach->intent, attach->attribute, attach->set);
    } else if (const auto* riding = std::get_if<MotionOperation::Riding>(&op)) {
        const std::optional<std::uint32_t> category =
            host.condition_inputs.physical_state ? std::optional<std::uint32_t>(host.condition_inputs.physical_state->category)
                                                 : std::nullopt;
        const std::optional<float> height = host.crouching_physical
                                                ? std::optional<float>(host.crouching_physical->animation_height_72)
                                                : std::nullopt;
        S3_MHOST_CHECK(host.riding.execute(riding->_0, phase, frame, host.animation, category, height,
                                            host.flags.manualing));
    } else if (operation.is<MotionOperation::ApplyingBodyTilt>()) {
        if (phase == 0) {
            host.applying_body_tilt = true;
        } else if (phase == 2) {
            host.applying_body_tilt = false;
        }
    } else if (const auto* crouch = std::get_if<MotionOperation::Crouching>(&op)) {
        namespace crouching = s3::animation::crouching;
        auto* state = instance.get<Instance::Crouching>();
        if (state == nullptr) return invalid();
        if (phase == 0) {
            S3_MHOST_SOME(const auto physical, host.crouching_physical,
                           "Crouching Begin requires real physical height and speed");
            state->_0 = crouching::State::begin(physical, host.crouching);
        } else if (phase == 1) {
            crouching::Intents intents{};
            intents.auto_pump_angle = host.animation.motion_intent("AutoPumpAngle");
            intents.auto_pump_magnitude = host.animation.motion_intent("AutoPumpMag");
            intents.crouch = host.animation.motion_intent("Crouch");
            intents.hard_turn_crouch = host.animation.motion_intent("HardTurnCrouch");
            intents.manual = host.animation.motion_intent("Manual");
            intents.motion_flag_108 = host.is_power_sliding;
            if (!state->_0) return rs::make_err(std::string("Crouching updated before Begin"));
            S3_MHOST_SOME(const auto physical, host.crouching_physical,
                           "Crouching requires actual physical outputs");
            const crouching::Output result = state->_0->update(physical, intents, frame.dt, host.crouching);
            if (result.new_auto_pump) host.animation.emit_packet(encode("NewAutoPump"), 1.0f);
            if (result.player_controlled_pump) host.animation.emit_packet(encode("PlayerControlledPump"), 1.0f);
            host.animation.set_attribute(attribute(crouch->_0, result.height));
        }
    } else if (const auto* turning = std::get_if<MotionOperation::SetTurning>(&op)) {
        namespace set_turning = s3::input::set_turning;
        auto* state = instance.get<Instance::Turning>();
        if (state == nullptr) return invalid();
        if (phase == 0) {
            state->_0.enter();
        } else if (phase == 1) {
            S3_MHOST_SOME(const MotionPhysical physical, host.physical,
                           "SetTurning requires actual PhysOutAnimation and stance outputs");
            set_turning::Intents intents{};
            intents.fakie_turn = host.animation.motion_intent("FakieTurn");
            intents.mode_0_slide = host.animation.motion_intent("LeftSlide");
            intents.mode_1_slide = host.animation.motion_intent("RightSlide");
            //ISkaterAnim v12=82B970D8/v28=82B97140 read live bits29/30.
            //UpdateRidingFakie may have changed them earlier in this graph tick.
            S3_MHOST_SOME(const std::uint32_t flags, host.animation.skater_animation_flags,
                           "SetTurning requires actual animation stance flags");
            const auto& names = turning->_0;
            set_turning::update(
                state->_0, host.slide_latch, physical.turning,
                {(flags & 0x20000000u) != 0, (flags & 0x40000000u) != 0}, frame.dt, host.turning, intents,
                [&](set_turning::Attribute attribute_, float value) {
                    std::optional<std::size_t> index;
                    switch (attribute_) {
                        case set_turning::Attribute::Angle: index = 0; break;
                        case set_turning::Attribute::Direction: index = 1; break;
                        case set_turning::Attribute::Quickness: index = 2; break;
                        case set_turning::Attribute::Speed: index = 3; break;
                        case set_turning::Attribute::Holding: index = 4; break;
                        default: break;
                    }
                    if (index) {
                        host.animation.set_attribute(attribute(names[*index], value));
                    } else {
                        host.animation.emit_packet(
                            encode(attribute_ == set_turning::Attribute::Turn ? "Turn" : "Slide"), value);
                    }
                });
        }
    } else if (operation.is<MotionOperation::Push>()) {
        auto* state = instance.get<Instance::Push>();
        if (state == nullptr) return invalid();
        S3_MHOST_SOME(const MotionPhysical physical, host.physical,
                       "Push behavior requires actual skater physical outputs");
        // The attribute sink borrows animation mutably during this
        // callback; retain a snapshot of the same canonical intent map.
        const IntentMap intents = host.animation.motion_intents;
        if (!host.push_state) {
            return rs::make_err(std::string("Native push state initialization has not been published"));
        }
        PushContext context{host.pushing,
                            *host.push_state,
                            intents,
                            physical.forward_speed,
                            frame.dt,
                            host.riding.time_since_teleport,
                            physical.is_switch,
                            physical.foot_frame};
        switch (phase) {
            case 0: state->_0.begin(context, host.animation); break;
            case 1: S3_MHOST_CHECK(state->_0.update(context, host.animation)); break;
            default: state->_0.end(context); break;
        }
    } else if (const auto* unsupported = std::get_if<MotionOperation::Unsupported>(&op)) {
        return rs::make_err("Unsupported MotionGraph " + std::string(action_nodes::debug_kind(unsupported->kind)) +
                            " " + unsupported->name);
    } else {
        return invalid();
    }
    return rs::Unit{};
}

}  // namespace s3::game::graph_host::motion::leaf_execution
