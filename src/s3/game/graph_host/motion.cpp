// Port of skate-game/src/graph_host/motion.rs.
#include "s3/game/graph_host/motion.hpp"

#include "s3/core/animation/channel_playback.hpp"
#include "s3/core/riding/push_animation.hpp"
#include "s3/game/graph_host/action_nodes.hpp"
#include "s3/game/graph_host/crouching_settings.hpp"
#include "s3/game/graph_host/motion/execution.hpp"
#include "s3/game/graph_host/motion/fakie_hold.hpp"
#include "s3/game/graph_host/motion_air_leg.hpp"
#include "s3/game/graph_host/motion_conditions.hpp"
#include "s3/game/graph_host/motion_hooks.hpp"
#include "s3/game/graph_host/motion_kickturn.hpp"
#include "s3/game/graph_host/motion_manual.hpp"
#include "s3/game/graph_host/pumping_settings.hpp"
#include "s3/game/graph_host/turning_settings.hpp"

namespace s3::game::graph_host::motion {

namespace {

/// Rust `format!("{:?} `{name}`", kind)`.
std::string describe(s3::data::state_graph_binding::OperationKind kind, const std::string& name) {
    return std::string(action_nodes::debug_kind(kind)) + " `" + name + "`";
}

}  // namespace

void MotionHost::end_gesture_channels() {
    // EndGesture nodes share the graph-wide gesture channel owner. The
    // active CharacterGesture instance is found by its operation binding.
    for (Instance& instance : instances) {
        if (auto* state = instance.get<Instance::CharacterGesture>()) {
            state->_0.end(animation);
        }
    }
    gesture_publication = std::nullopt;
}

rs::Result<MotionHost, std::string> MotionHost::from_graph(const LoadedGraph& graph,
                                                           const s3::data::Collections& data,
                                                           s3::data::AnimationMetadata metadata,
                                                           PlaybackContext playback_context) {
    MotionFactory factory;
    auto instantiated = graph.binding.instantiate_operations<MotionOperation>(graph.source, factory);
    if (!instantiated.ok) return rs::make_err(std::string(instantiated.error));
    std::vector<MotionOperation> operations = std::move(instantiated.value.operations);
    motion_conditions::bind_states(graph, operations);
    std::vector<Instance> instances;
    instances.reserve(graph.runtime.operations.behaviors.size());
    for (std::size_t id : graph.runtime.operations.behaviors) instances.push_back(Instance::new_(operations[id]));
    GraphCapabilityReport capabilities;
    for (const MotionOperation& operation : operations) {
        if (!operation.is<MotionOperation::Unsupported>()) capabilities.supported_operations += 1;
    }
    for (const MotionOperation& operation : operations) {
        if (const auto* unsupported = std::get_if<MotionOperation::Unsupported>(&operation.value)) {
            capabilities.unsupported_operations.push_back(describe(unsupported->kind, unsupported->name));
        }
    }
    for (std::size_t operation : graph.runtime.operations.conditions) {
        if (operation < operations.size()) {
            if (const auto* u = std::get_if<MotionOperation::Unsupported>(&operations[operation].value)) {
                capabilities.unsupported_conditions.push_back(describe(u->kind, u->name));
            }
        }
    }
    for (std::size_t operation : graph.runtime.operations.hooks) {
        if (operation < operations.size()) {
            if (const auto* u = std::get_if<MotionOperation::Unsupported>(&operations[operation].value)) {
                capabilities.unsupported_hooks.push_back(describe(u->kind, u->name));
            }
        }
    }
    (void)capabilities;  // Built but unused in Rust as well.
    // Preserve lazy diagnostics for optional unsupported graph branches.
    //8258F488 and final reset825953B0 explicitly zero the complete push
    //state; reset clears manualing and body-tilt flag bits too.
    s3::riding::push_animation::PushBlendParameters zero{};
    zero.hstr_vel_b = 0.0f;
    zero.lstr_vel_b = 0.0f;
    zero.vel_e = 0.0f;
    PushState push_state{};
    push_state.out_factor = 0.0f;
    push_state.current_push_dv = 0.0f;
    push_state.current = zero;
    push_state.target = zero;
    push_state.continue_push = false;
    pushing_settings::AnimationMetadataTrees trees(metadata);
    S3_MHOST_TRY(PushingSettings pushing, PushingSettings::load(data, trees));

    // Struct-literal field order of the Rust constructor (fallible loaders run in this order).
    MotionHost host(MotionAnimation::from_metadata(std::move(metadata)));
    host.playback_context = std::move(playback_context);
    host.condition_inputs = ConditionInputs{};
    host.gameplay_conditions = std::nullopt;
    host.manual_exit = std::nullopt;
    host.deck_yaw_pitch = std::nullopt;
    host.riding_conditions = std::nullopt;
    host.native_physical = std::nullopt;
    host.prelanding_physical = std::nullopt;
    host.air_leg_physical = std::nullopt;
    host.gesture_physical = std::nullopt;
    host.gesture_publication = std::nullopt;
    host.toggle_board_physical = std::nullopt;
    host.runout_physical = std::nullopt;
    host.grind_physical = std::nullopt;
    host.grind_conditions = std::nullopt;
    S3_MHOST_TRY(host.grind_settings, motion_grind::Settings::load(data));
    host.shove_physical = std::nullopt;
    host.is_power_sliding = false;
    host.trick_height_settings = {true, true};
    host.action_controls = ActionControls{};
    host.action_intents = IntentMap::new_();
    host.turning_output = TurningOutput{};
    host.graph_effects = GraphEffects{};
    host.time_tags = std::nullopt;
    host.physical = std::nullopt;
    host.offboard_cadence_phase = std::nullopt;
    host.offboard_locomotion_state = std::nullopt;
    host.ground_slope_type = std::nullopt;
    host.biped_ground_thin = std::nullopt;
    host.animation_phase = 0.0f;
    host.push_state = push_state;
    host.applying_body_tilt = false;
    host.crouching_physical = std::nullopt;
    host.body_tilt_physical = std::nullopt;
    host.hold_fakie = false;
    host.automatic_fakie_conditions = fakie_hold::bind(graph.binding);
    host.fakie_physical = std::nullopt;
    host.physical_stance = std::nullopt;
    host.landing_physical = std::nullopt;
    host.wipeout_physical = std::nullopt;
    S3_MHOST_TRY(host.wipeout_settings, motion_wipeout::Settings::load(data));
    host.pumping_acceleration = std::nullopt;
    host.bump_acceleration = std::nullopt;
    S3_MHOST_TRY(host.bump_settings, motion_bump::Settings::load(data));
    //8258F488 seeds bit24, and reset825953B0 preserves that bit.
    host.allow_pumping = true;
    host.riding = motion_riding::RidingState::new_();
    host.errors = GraphDiagnostics{};
    host.state_parents.clear();
    for (const auto& state : graph.binding.states) host.state_parents.push_back(state.parent);
    host.operations = std::move(operations);
    host.instances = std::move(instances);
    host.remap = graph.runtime.operations;
    host.pushing = std::move(pushing);
    S3_MHOST_TRY(host.turning, turning_settings::load(data));
    S3_MHOST_TRY(host.crouching, crouching_settings::load(data));
    S3_MHOST_TRY(host.body_tilt, motion_riding::body_tilt_settings(data));
    S3_MHOST_TRY(host.pumping, pumping_settings::load(data));
    S3_MHOST_TRY(host.sliding, motion_sliding::Settings::load(data));
    S3_MHOST_TRY(host.spin, motion_spin::Settings::load(data));
    S3_MHOST_TRY(host.air_leg, motion_air_leg::load(data));
    S3_MHOST_TRY(host.kickturn, motion_kickturn::load_settings(data));
    S3_MHOST_TRY(host.manual, motion_manual::settings(data));
    S3_MHOST_TRY(host.hippy_jump, motion_hippy_jump::Settings::load(data));
    S3_MHOST_TRY(host.finger_flip, motion_finger_flip::Settings::load(data));
    host.next_instance = 1;
    return std::move(host);
}

void MotionHost::accept_action_graph(MotionGraphInput input) {
    const std::uint64_t tick = input.tick;
    outputs::ActionGraphOutput action = std::move(input.action);
    (void)tick;  // debug_assert_eq!(tick, action.tick);
    errors.clear();
    turning_output = action.turning;
    graph_effects = action.effects;
    action_controls = action.controls;
    // Clone before handing the full output to MotionAnimation so the two
    // consumers observe one immutable packet.
    action_intents = action.controls.authored_values();
    animation.accept_action_graph(std::move(action));
}

void MotionHost::run(BehaviorId id, const Frame& frame, std::uint8_t phase) {
    auto result = execute(id, frame, phase);
    if (result.is_err()) {
        errors.push("MotionGraph behavior " + std::to_string(id) + ": " + result.unwrap_err());
    }
}

std::uint32_t MotionHost::condition_activation(std::size_t condition, const Frame& frame) {
    // Gate only the authored automatic switch transitions. Other fakie
    // conditions (tricks, pushes, dismounts) still see the real stance.
    if (condition < remap.conditions.size() &&
        fakie_hold::blocked(hold_fakie, automatic_fakie_conditions, remap.conditions[condition])) {
        return 0;
    }
    rs::Result<bool, std::string> result = [&]() -> rs::Result<bool, std::string> {
        if (condition >= remap.conditions.size() || remap.conditions[condition] >= operations.size()) {
            return rs::make_err(std::string("Unbound MotionGraph condition"));
        }
        const MotionOperation& op = operations[remap.conditions[condition]];
        if (const auto* motion_condition = std::get_if<MotionOperation::Condition>(&op.value)) {
            return motion_condition->_0.evaluate(*this, frame);
        }
        return rs::make_err("Unsupported MotionGraph condition " + execution::debug_operation(op));
    }();
    if (result.is_ok()) return static_cast<std::uint32_t>(result.unwrap());
    errors.push(result.unwrap_err());
    return 0;
}

std::uint32_t MotionHost::allocate(BehaviorId behavior, const Frame& /*frame*/) {
    if (behavior < remap.behaviors.size()) {
        instances[behavior] = Instance::new_(operations[remap.behaviors[behavior]]);
    }
    const std::uint32_t handle = next_instance;
    const std::uint32_t next = next_instance + 1u;  // wrapping_add(1).max(1)
    next_instance = next < 1u ? 1u : next;
    return handle;
}

void MotionHost::begin(BehaviorId id, std::array<std::uint32_t, 6> /*context*/, const Frame& frame) {
    run(id, frame, 0);
}

void MotionHost::update(BehaviorId id, std::array<std::uint32_t, 6> /*context*/, const Frame& frame) {
    run(id, frame, 1);
}

void MotionHost::end(BehaviorId id, std::array<std::uint32_t, 6> /*context*/, const Frame& frame) {
    run(id, frame, 2);
}

void MotionHost::hook(HookId hook, const Frame& /*frame*/) {
    const MotionOperation* bound = nullptr;
    if (hook < remap.hooks.size() && remap.hooks[hook] < operations.size()) bound = &operations[remap.hooks[hook]];
    if (bound != nullptr) {
        if (const auto* hook_op = std::get_if<MotionOperation::Hook>(&bound->value)) {
            const motion_hooks::MotionHook operation = hook_op->_0;
            if (const auto* grab = std::get_if<motion_hooks::MotionHook::GrabSlide>(&operation.value)) {
                slide_latch.grab(grab->right);
            } else if (const auto* settings = std::get_if<motion_hooks::MotionHook::Override>(&operation.value)) {
                playback_context.transition_override = settings->_0;
            } else if (const auto* mongo = std::get_if<motion_hooks::MotionHook::MongoPushToAntic>(&operation.value)) {
                //82BBBBB8 passes the authored name for both channel/tree.
                s3::animation::channel_playback::ChannelSettings channel{};
                channel.priority = 0;
                channel.keep_alive = false;
                channel.mirrored = false;
                channel.speed = 1.0f;
                channel.blend_in = rs::from_bits(0x3dcccccd);
                channel.hold_during_blend_in = false;
                channel.blend_out = rs::from_bits(0x3d8f5c29);
                channel.hold_during_blend_out = false;
                channel.use_attributes = false;
                auto result = animation.new_channel(mongo->animation, mongo->animation, channel);
                if (result.is_err()) errors.push(std::string(result.unwrap_err()));
            }
            return;
        }
    }
    errors.push("Unsupported MotionGraph hook " +
                (bound ? "Some(" + execution::debug_operation(*bound) + ")" : std::string("None")));
}

}  // namespace s3::game::graph_host::motion
