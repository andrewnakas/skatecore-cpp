// Port of skate-game/src/graph_host/action.rs.
#include "s3/game/graph_host/action.hpp"

#include <cstdio>

#include "s3/data/result.hpp"
#include "s3/game/graph_host/condition_nodes.hpp"

namespace s3::game::graph_host::action {

namespace {

/// Rust `format!("{:?} `{}`", operation.kind, operation.name)`.
std::string describe(const action_nodes::UnsupportedOperation& operation) {
    return std::string(action_nodes::debug_kind(operation.kind)) + " `" + operation.name + "`";
}

/// Rust `{:?}` of `Option<String>`.
std::string debug_option(const std::optional<std::string>& value) {
    return value ? "Some(" + s3::data::debug_str(*value) + ")" : std::string("None");
}

}  // namespace

rs::Result<ActionHost, std::string> ActionHost::from_graph(const LoadedGraph& graph,
                                                           const s3::data::Collections& data) {
    ActionFactory factory;
    auto operations = graph.binding.instantiate_operations<ActionInstance>(graph.source, factory);
    if (!operations.ok) return rs::make_err(operations.error);
    ActionInstances instances = ActionInstances::new_(std::move(operations.value.operations));
    GraphCapabilityReport capabilities;
    for (const ActionInstance& instance : instances.operations) {
        if (!instance.unsupported()) capabilities.supported_operations += 1;
    }
    for (const ActionInstance& instance : instances.operations) {
        if (const auto operation = instance.unsupported()) {
            capabilities.unsupported_operations.push_back(describe(*operation));
        }
    }
    for (std::size_t operation : graph.runtime.operations.conditions) {
        if (const ActionInstance* instance = instances.get(operation)) {
            if (const auto unsupported = instance->unsupported()) {
                capabilities.unsupported_conditions.push_back(describe(*unsupported));
            }
        }
    }
    for (std::size_t operation : graph.runtime.operations.hooks) {
        if (const ActionInstance* instance = instances.get(operation)) {
            if (const auto unsupported = instance->unsupported()) {
                capabilities.unsupported_hooks.push_back(describe(*unsupported));
            }
        }
    }
    (void)capabilities;  // Built but unused in Rust as well.
    // Preserve lazy diagnostics for optional unsupported graph branches.
    condition_nodes::bind_current_states(graph, instances);
    ActionHost host = ActionHost::new_(std::move(instances), graph.runtime.operations);
    //82BA35E0/3790 read Globals296;8289F8F0 binds hash
    //3FC308E69AEA6385 (body_flip), independently checked in stock data.
    auto gesture_window = data.float_("anim_motion", "body_flip", "extend_bodyflip_gesture");
    if (!gesture_window.ok) return rs::make_err(gesture_window.error);
    auto takeoff_window = data.float_("anim_motion", "body_flip", "extend_takeoff_point");
    if (!takeoff_window.ok) return rs::make_err(takeoff_window.error);
    host.body_flip_settings = s3::input::body_flip_signal::Settings{gesture_window.value, takeoff_window.value};
    host.state_parents.clear();
    for (const auto& state : graph.binding.states) host.state_parents.push_back(state.parent);
    return host;
}

ActionHost ActionHost::new_(ActionInstances instances, OperationRemap remap) {
    const std::size_t count = remap.behaviors.size();
    ActionHost host;
    host.const_handlers.reserve(count);
    for (std::size_t operation : remap.behaviors) {
        const ActionInstance& instance = instances.operations[operation];
        host.const_handlers.push_back(ConstMgIntent::new_(
            instance.config.float_bits ? rs::from_bits(*instance.config.float_bits) : 0.0f, instance.config.on_update));
    }
    host.time_handlers.assign(count, TimeMgIntent{});
    host.instances = std::move(instances);
    host.action_intents = IntentMap::new_();
    host.motion_intents = IntentMap::new_();
    host.condition_inputs = ConditionInputs{};
    host.physical_conditions = std::nullopt;
    host.gameplay_conditions = std::nullopt;
    host.animation_attributes.clear();
    host.is_tricking = std::nullopt;
    host.dropping_in = std::nullopt;
    host.state_parents.clear();
    host.remap = std::move(remap);
    host.stance = std::nullopt;
    host.tick = 0;
    host.errors = GraphDiagnostics{};
    host.next_instance = 1;
    host.created.assign(count, false);
    host.board_adjust.assign(count, board_adjust::State{});
    host.body_flip.assign(count, s3::input::body_flip_signal::State{});
    host.body_flip_settings = std::nullopt;
    host.juice_pending.assign(count, std::string());
    host.trick_handlers.assign(count, s3::game::input::gesture_mapping::State::default_());
    return host;
}

void ActionHost::apply(std::string name, IntentMutation mutation) {
    if (const auto* set = std::get_if<IntentMutation::Set>(&mutation.value)) {
        motion_intents.insert(name, set->_0);
    } else if (std::holds_alternative<IntentMutation::Remove>(mutation.value)) {
        motion_intents.remove(name);
    }
}

void ActionHost::prepare_input(ActionGraphInput input) {
    errors.clear();
    tick = input.tick;
    action_intents = input.controls.into_values();
    motion_intents = input.prior_motion.into_values();
    animation_attributes = std::move(input.animation_attributes);
}

ActionGraphOutput ActionHost::output() const {
    return ActionGraphOutput::from_host(tick, action_intents, motion_intents, animation_attributes);
}

const ActionInstance* ActionHost::operation(BehaviorId id) const {
    if (id >= remap.behaviors.size()) return nullptr;
    return instances.get(remap.behaviors[id]);
}

void ActionHost::unsupported(BehaviorId id, const ActionInstance& instance) {
    if (const auto error = instance.unsupported()) {
        errors.push("ActionGraph behavior " + std::to_string(id) + ": " + describe(*error));
    }
}

void ActionHost::publish_board_adjust(BehaviorId behavior, const ActionInstance& instance) {
    if (!instance.config.mg_intent_mag) return;
    const std::string& magnitude_name = *instance.config.mg_intent_mag;
    if (!instance.config.mg_intent_angle) return;
    const std::string& angle_name = *instance.config.mg_intent_angle;

    auto copied = [](const float* value) { return value ? std::optional<float>(*value) : std::nullopt; };
    const auto result = board_adjust[behavior].update(
        copied(action_intents.get("BoardAdjustMag")), copied(action_intents.get("BoardAdjustAngle")),
        instance.config.angle_filter, instance.config.negate_on_mirror, stance && stance->second);
    if (result) {
        motion_intents.insert(magnitude_name, result->first);
        motion_intents.insert(angle_name, result->second);
    } else {
        motion_intents.remove(magnitude_name);
        motion_intents.remove(angle_name);
    }
}

std::uint32_t ActionHost::condition_activation(std::size_t condition, const Frame& frame) {
    rs::Result<bool, std::string> result = [&]() -> rs::Result<bool, std::string> {
        const ActionInstance* instance =
            condition < remap.conditions.size() ? instances.get(remap.conditions[condition]) : nullptr;
        if (instance == nullptr) {
            return rs::make_err("ActionGraph condition " + std::to_string(condition) + " is unbound");
        }
        const ActionOperation& op = instance->operation;
        if (const auto* extra = std::get_if<ActionOperation::ExtraCondition>(&op.value)) {
            return extra->_0.evaluate(*this, frame);
        }
        if (op.is<ActionOperation::PhysicsRequestsDismount>()) {
            if (!physical_conditions) {
                return rs::make_err(std::string("PhysicsRequestsDismount requires published State77"));
            }
            return physical_conditions->requests_dismount;
        }
        if (op.is<ActionOperation::IsLandingOnBoard>()) {
            if (!physical_conditions) return rs::make_err(std::string("IsLandingOnBoard requires actual State16"));
            return physical_conditions->state == 503;
        }
        if (const auto* core = std::get_if<ActionOperation::Condition>(&op.value)) {
            auto evaluated = core->_0.evaluate(condition_inputs, action_intents, frame.current, state_parents);
            if (evaluated.is_err()) {
                return rs::make_err("ActionGraph condition " + std::to_string(condition) + ": " +
                                    std::string(evaluated.unwrap_err()));
            }
            return evaluated.unwrap();
        }
        return rs::make_err("Unsupported ActionGraph condition " + std::to_string(condition) + ": " +
                            debug_option(instance->config.name));
    }();
    if (result.is_ok()) return static_cast<std::uint32_t>(result.unwrap());
    errors.push(result.unwrap_err());
    return 0;
}

std::uint32_t ActionHost::allocate(BehaviorId behavior, const Frame& /*frame*/) {
    // Every activation owns fresh behavior state. Re-entry must not reuse
    // the old pushing clock, even if the source operation ID is unchanged.
    created[behavior] = false;
    time_handlers[behavior] = TimeMgIntent{};
    body_flip[behavior] = s3::input::body_flip_signal::State{};
    juice_pending[behavior].clear();
    trick_handlers[behavior] = s3::game::input::gesture_mapping::State::default_();
    const std::uint32_t result = next_instance;
    const std::uint32_t next = next_instance + 1u;  // wrapping_add
    next_instance = next > 1u ? next : 1u;           // .max(1)
    return result;
}

void ActionHost::begin(BehaviorId behavior, std::array<std::uint32_t, 6> /*context*/, const Frame& /*frame*/) {
    const ActionInstance* found = operation(behavior);
    if (found == nullptr) return;
    const ActionInstance instance = *found;  // .cloned()
    if (const auto* trick = std::get_if<ActionOperation::CreateTrickIntentFromGesture>(&instance.operation.value)) {
        if (stance) {
            trick_handlers[behavior].begin(
                trick->group,
                trick->override_name ? std::optional<std::string_view>(*trick->override_name) : std::nullopt,
                action_intents, motion_intents, stance->second);
        } else {
            errors.push("CreateTrickIntentFromGesture requires published skater stance");
        }
        return;
    }
    if (instance.unsupported()) {
        unsupported(behavior, instance);
        return;
    }
    if (const auto* print = std::get_if<ActionOperation::PrintText>(&instance.operation.value)) {
        std::fprintf(stderr, "INFO ActionGraph: %s\n", print->_0.c_str());
        return;
    }
    if (instance.operation.is<ActionOperation::BodyFlippingSignal>()) {
        if (body_flip_settings) {
            body_flip[behavior].begin(*body_flip_settings);
        } else {
            errors.push("BodyFlippingSignal requires stock anim_motion settings");
        }
        return;
    }
    if (instance.operation.is<ActionOperation::BoardAdjust>()) {
        board_adjust[behavior].begin();
        return;
    }
    if (instance.operation.is<ActionOperation::CreateConstMgIntent>()) {
        if (instance.config.mg_intent) {
            const std::string name = *instance.config.mg_intent;
            const IntentMutation mutation = const_handlers[behavior].begin();
            apply(name, mutation);
        }
    }
    if (instance.operation.is<ActionOperation::CreateMgIntent>()) {
        if (instance.config.mg_intent && instance.config.ag_intent) {
            const std::string mg = *instance.config.mg_intent;
            const std::string& ag = *instance.config.ag_intent;
            CreateMgIntent handler;
            handler.on_update = instance.config.on_update;
            handler.default_value = instance.config.default_value;
            handler.scale = instance.config.scale ? *instance.config.scale : 1.0f;
            handler.filters = instance.config.filters;
            const float* value_ptr = action_intents.get(ag);
            const std::optional<float> value = value_ptr ? std::optional<float>(*value_ptr) : std::nullopt;
            bool created_flag = created[behavior];
            const IntentMutation mutation = handler.enter(created_flag, value, stance);
            created[behavior] = created_flag;
            apply(mg, mutation);
        } else {
            errors.push("ActionGraph behavior " + std::to_string(behavior) + ": missing AGIntent/MGIntent");
        }
    }
}

void ActionHost::update(BehaviorId behavior, std::array<std::uint32_t, 6> /*context*/, const Frame& frame) {
    const ActionInstance* found = operation(behavior);
    if (found == nullptr) return;
    const ActionInstance instance = *found;  // .cloned()
    if (instance.operation.is<ActionOperation::CreateTrickIntentFromGesture>()) {
        trick_handlers[behavior].update(motion_intents);
        return;
    }
    if (instance.unsupported()) {
        unsupported(behavior, instance);
        return;
    }
    if (instance.operation.is<ActionOperation::JuiceHook>()) {
        motion_intents.remove(juice_pending[behavior]);
        juice_pending[behavior].clear();
        return;
    }
    if (instance.operation.is<ActionOperation::BodyFlippingSignal>()) {
        if (!body_flip_settings) {
            errors.push("BodyFlippingSignal requires stock anim_motion settings");
            return;
        }
        const auto settings = *body_flip_settings;
        if (!condition_inputs.physical_state) {
            errors.push("BodyFlippingSignal requires published filtered state");
            return;
        }
        const auto& physical = *condition_inputs.physical_state;
        // TU3 initializers82F84DA8/82F84DC0 bind these exact keys.
        const std::array<const char*, 2> names = {"FrontFlip", "BackFlip"};
        const std::array<bool, 2> present = {action_intents.contains_key(names[0]),
                                             action_intents.contains_key(names[1])};
        if (const auto index = body_flip[behavior].update(present, physical.category, frame.dt, settings)) {
            motion_intents.insert(names.at(*index), 1.0f);
        } else {
            for (const char* name : names) motion_intents.remove(name);
        }
        return;
    }
    if (instance.operation.is<ActionOperation::BoardAdjust>()) {
        publish_board_adjust(behavior, instance);
        return;
    }
    if (instance.operation.is<ActionOperation::CreateConstMgIntent>()) {
        if (instance.config.mg_intent) {
            const IntentMutation mutation = const_handlers[behavior].update();
            apply(*instance.config.mg_intent, mutation);
        }
        return;
    }
    if (!instance.config.ag_intent) return;
    const std::string& ag_name = *instance.config.ag_intent;
    if (!instance.config.mg_intent) return;
    const std::string& mg_name = *instance.config.mg_intent;
    const float* ag_ptr = action_intents.get(ag_name);
    const std::optional<float> ag = ag_ptr ? std::optional<float>(*ag_ptr) : std::nullopt;
    IntentMutation mutation;
    if (instance.operation.is<ActionOperation::CreateMgIntent>()) {
        CreateMgIntent handler;
        handler.on_update = instance.config.on_update;
        handler.default_value = instance.config.default_value;
        handler.scale = instance.config.scale ? *instance.config.scale : 1.0f;
        handler.filters = instance.config.filters;
        bool created_flag = created[behavior];
        mutation = handler.update(created_flag, ag, stance);
        created[behavior] = created_flag;
    } else if (instance.operation.is<ActionOperation::CreateMgTimeIntent>()) {
        mutation = time_handlers[behavior].update(ag, frame.dt);
    } else {
        return;
    }
    apply(mg_name, mutation);
}

void ActionHost::end(BehaviorId behavior, std::array<std::uint32_t, 6> /*context*/, const Frame& /*frame*/) {
    const ActionInstance* found = operation(behavior);
    if (found == nullptr) return;
    const ActionInstance instance = *found;  // .cloned()
    if (instance.operation.is<ActionOperation::BoardAdjust>()) {
        //82BA2EB8: remove both authored names; End does not reset state.
        for (const auto* name : {&instance.config.mg_intent_mag, &instance.config.mg_intent_angle}) {
            if (*name) motion_intents.remove(**name);
        }
        return;
    }
    if (instance.operation.is<ActionOperation::CreateTrickIntentFromGesture>()) {
        trick_handlers[behavior].end(motion_intents);
        return;
    }
    if (instance.config.mg_intent) {
        IntentMutation mutation = IntentMutation::Remove{};
        if (instance.operation.is<ActionOperation::CreateConstMgIntent>()) {
            mutation = const_handlers[behavior].end();
        } else if (instance.operation.is<ActionOperation::CreateMgTimeIntent>()) {
            mutation = time_handlers[behavior].end();
        }
        apply(*instance.config.mg_intent, mutation);
    }
}

void ActionHost::hook(HookId hook, const Frame& /*frame*/) {
    const ActionInstance* found = hook < remap.hooks.size() ? instances.get(remap.hooks[hook]) : nullptr;
    if (found == nullptr) {
        errors.push("ActionGraph hook index " + std::to_string(hook) + " is unbound");
        return;
    }
    const ActionInstance instance = *found;  // .cloned()
    if (instance.unsupported()) unsupported(hook, instance);
}

}  // namespace s3::game::graph_host::action
