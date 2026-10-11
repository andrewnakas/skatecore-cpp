// Port of simref sim/src/camera/graph.rs.
#include "s3/game/camera/graph.hpp"

#include <iostream>

#include "s3/game/camera/_result.hpp"

namespace s3::game::camera::graph {

namespace cam = s3::camera;
using s3::data::StateGraph;
using s3::data::state_graph_attributes::Attributes;
using s3::data::state_graph_binding::Binding;
using s3::game::graph_runtime::CompiledGraph;
using s3::graph::controller::BehaviorId;
using s3::graph::controller::Frame;
using s3::graph::controller::HookId;

namespace {

struct CameraHost final : s3::graph::controller::Host {
    cam::CameraMan* manager;
    const cam::ManagerSubject* subject;
    CameraGraphSubject physical;
    const CameraGraphEnvironment* world;
    const shot_data::StockShots* database;
    const std::vector<graph_conditions::Condition>* conditions;
    const std::vector<Behavior>* behaviors;
    std::optional<std::string> error;
    std::vector<std::optional<cam::SlowMotionController>>* slow_motion;
    cam::SlowMotionSettings slow_motion_settings;
    std::vector<cam::SimulationRateRequest> rates;

    CameraHost(cam::CameraMan& manager_, const cam::ManagerSubject& subject_, CameraGraphSubject physical_,
               const CameraGraphEnvironment& world_, const shot_data::StockShots& database_,
               const std::vector<graph_conditions::Condition>& conditions_, const std::vector<Behavior>& behaviors_,
               std::vector<std::optional<cam::SlowMotionController>>& slow_motion_,
               cam::SlowMotionSettings slow_motion_settings_)
        : manager(&manager_),
          subject(&subject_),
          physical(physical_),
          world(&world_),
          database(&database_),
          conditions(&conditions_),
          behaviors(&behaviors_),
          slow_motion(&slow_motion_),
          slow_motion_settings(slow_motion_settings_) {}

    // impl ConditionHost
    std::uint32_t condition_activation(std::size_t id, const Frame&) override {
        return (*this->conditions)[id].evaluate(*this->manager, *this->subject, this->physical, *this->world) ? 1u
                                                                                                             : 0u;
    }
    // impl Host
    std::array<std::uint32_t, 6> context() const override { return {0, 0, 0, 0, 0, 0}; }
    std::uint32_t allocate(BehaviorId, const Frame&) override { return 0; }
    void begin(BehaviorId id, std::array<std::uint32_t, 6>, const Frame&) override {
        const Behavior& behavior = (*this->behaviors)[id];
        if (const auto* choose = std::get_if<Behavior::Choose>(&behavior.value)) {
            if (choose->names.empty()) {
                this->error = "CameraChooseShot has no named shot";
                return;
            }
            const std::string& name = choose->names.front();
            auto set = this->manager->set_shot(name, false, *this->subject, *this->database);
            if (set.is_err()) {
                this->error = set.unwrap_err();
                return;
            }
            this->manager->shots.apply_graph_transition(choose->incoming, choose->outgoing);
        } else if (const auto* print = std::get_if<Behavior::Print>(&behavior.value)) {
            std::cerr << "WARN Stock camera graph: " << print->_0 << "\n"; /* SIMREF */
        } else {
            auto [instance, request] = cam::SlowMotionController::begin(this->slow_motion_settings);
            (*this->slow_motion)[id] = instance;
            this->rates.push_back(request);
        }
    }
    // CameraChooseShot Update/End both resolve to TU382B61BB8 (return).
    void update(BehaviorId id, std::array<std::uint32_t, 6>, const Frame& frame) override {
        auto& instance = (*this->slow_motion)[id];
        if (instance.has_value()) {
            this->rates.push_back(
                instance->update(frame.dt, this->physical.slow_motion_air_duration, this->slow_motion_settings));
        }
    }
    void end(BehaviorId id, std::array<std::uint32_t, 6>, const Frame&) override {
        auto& instance = (*this->slow_motion)[id];
        if (instance.has_value()) {
            instance.reset();
            this->rates.push_back(cam::SlowMotionController::end());
        }
    }
    void hook(HookId, const Frame&) override { S3_PANIC("internal error: entered unreachable code: camera hooks rejected during load"); }
    void release(std::uint32_t) override {}
};

}  // namespace

rs::Result<CameraGraph, std::string> CameraGraph::load(const s3::data::Vfs& vfs, const std::string& path,
                                                       cam::SlowMotionSettings slow_motion_settings) {
    S3_CAM_TRY_DATA(StateGraph source, StateGraph::load(vfs, path));
    S3_CAM_TRY_DATA(Binding binding, Binding::from_graph(source));
    auto compiled = CompiledGraph::from_binding(binding);
    if (compiled.is_err()) {
        return rs::make_err(compiled.unwrap_err().to_string());
    }
    CompiledGraph program = std::move(compiled.unwrap());
    std::vector<graph_conditions::Condition> conditions;
    for (std::size_t id : program.operations.conditions) {
        const auto& node = source.elements[binding.operations[id].element];
        S3_CAM_TRY(auto condition, graph_conditions::Condition::parse(Attributes(node.attributes)));
        conditions.push_back(std::move(condition));
    }
    std::vector<Behavior> behaviors;
    for (std::size_t id : program.operations.behaviors) {
        const auto& operation = binding.operations[id];
        const auto& node = source.elements[operation.element];
        const Attributes a(node.attributes);
        if (operation.name == "CameraChooseShot") {
            std::vector<std::string> names = graph_conditions::shot_names(a);
            if (names.empty()) {
                return rs::make_err(std::string("CameraChooseShot has no named shot"));
            }
            behaviors.push_back(Behavior{Behavior::Choose{
                std::move(names),
                rs::from_bits(a.float_bits("transitionIn", rs::to_bits(-1.0f))),
                rs::from_bits(a.float_bits("transitionOut", rs::to_bits(-1.0f))),
            }});
        } else if (operation.name == "PrintText2D") {
            const std::string* text = a.text("text");
            behaviors.push_back(Behavior{Behavior::Print{text != nullptr ? *text : std::string()}});
        } else if (operation.name == "SlowMotionController") {
            behaviors.push_back(Behavior{Behavior::SlowMotion{}});
        } else {
            return rs::make_err("Unimplemented camera behavior " + operation.name);
        }
    }
    if (!program.operations.hooks.empty()) {
        return rs::make_err(std::string("Camera graph has unimplemented transition hooks"));
    }
    CameraGraph out;
    out.controller = s3::graph::controller::Controller::new_(binding.states.size());
    out.slow_motion = std::vector<std::optional<cam::SlowMotionController>>(behaviors.size());
    out.program = std::move(program);
    out.conditions = std::move(conditions);
    out.behaviors = std::move(behaviors);
    out.slow_motion_settings = slow_motion_settings;
    return out;
}

rs::Result<std::vector<cam::SimulationRateRequest>, std::string> CameraGraph::update(
    float dt, cam::CameraMan& manager, const cam::ManagerSubject& subject, const CameraGraphSubject& physical,
    const CameraGraphEnvironment& world, const shot_data::StockShots& database) {
    CameraHost host(manager, subject, physical, world, database, this->conditions, this->behaviors, this->slow_motion,
                    this->slow_motion_settings);
    this->controller.update(this->program.program, dt, host);
    if (host.error.has_value()) {
        return rs::make_err(*host.error);
    }
    return std::move(host.rates);
}

}  // namespace s3::game::camera::graph
