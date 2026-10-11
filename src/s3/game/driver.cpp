// Port of simref crates/sim/src/driver.rs.
#include "s3/game/driver.hpp"

#include "s3/core/physics/board.hpp"
#include "s3/core/physics/phase.hpp"
#include "s3/data/gesture_patterns.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/difficulty.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/input.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/input/platform.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/frame.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/skate_world.hpp"
#include "s3/game/state_names.hpp"

namespace s3::game::driver {

using sc::input::xbox::XboxState;
using sc::physics::board::BodyId;
using sc::physics::phase::PhysicsBody;
using sc::physics::phase::PhysicsEvent;
using physics::GamePhysics;
using physics::PlayerControls;
using physics::SkaterRuntime;

struct Sim::State {
    std::optional<GamePhysics> physics;
    std::optional<SkaterRuntime> skater;
    std::optional<PlayerControls> controls;
    std::optional<graph_runtime::StockGraphs> graphs;
    std::optional<camera::CameraRuntime> camera;
    input::controllers::ControllerInput controller{};
    std::uint32_t packet_number = 0;
    std::optional<XboxState> last_pad;
    // Edge detection for the events column (observation only).
    std::string prev_motion;
    std::string prev_action;
    std::string prev_clip;
    std::optional<sc::animation::output::attributes::AttributeName> prev_trick;
    std::string prev_score_name;
    bool prev_wiping = false;
    std::uint32_t prev_state_id = 0;
};

Sim::Sim() : s_(std::make_unique<State>()) {}
Sim::~Sim() = default;

std::string decode_name(const sc::animation::output::attributes::AttributeName& name) {
    // Inverse of skate_core::animation::skeleton_input::name::encode (base 38, 6 digits/word).
    std::string out;
    for (std::uint32_t word : name._0) {
        std::uint32_t weight = 79235168u;
        std::uint32_t w = word;
        for (int i = 0; i < 6; ++i) {
            const std::uint32_t digit = w / weight;
            w %= weight;
            weight /= 38;
            if (digit == 0) {
            } else if (digit >= 1 && digit <= 10) {
                out.push_back(char('0' + (digit - 1)));
            } else if (digit >= 11 && digit <= 36) {
                out.push_back(char('A' + (digit - 11)));
            } else if (digit == 37) {
                out.push_back('_');
            } else {
                out.push_back('?');
            }
        }
    }
    return out;
}

namespace {

std::string state_path(const sd::state_graph_binding::Binding& binding, std::optional<std::size_t> id) {
    std::vector<std::string> names;
    auto cursor = id;
    while (cursor.has_value()) {
        const std::size_t i = *cursor;
        if (i < binding.states.size()) {
            names.push_back(binding.states[i].name);
            cursor = binding.states[i].parent;
        } else {
            names.push_back("?");
            break;
        }
    }
    std::string out;
    for (auto it = names.rbegin(); it != names.rend(); ++it) {
        if (!out.empty()) out += "/";
        out += *it;
    }
    return out;
}

std::array<float, 3> v3(sc::math::Vector3 v) { return {v.x, v.y, v.z}; }

}  // namespace

rs::Result<std::unique_ptr<Sim>, std::string> Sim::load(const sd::Vfs& vfs, const std::string& asset_root,
                                                        const sd::SkateMap* map, const char* difficulty_arg) {
    // main.rs: config.difficulty = --difficulty or Difficulty::load(asset_root) (default Easy)
    difficulty::Difficulty difficulty = difficulty::Difficulty::Easy;
    if (difficulty_arg != nullptr) {
        S3G_TRY(difficulty, difficulty::parse(difficulty_arg));
    } else {
        S3G_TRY(difficulty, difficulty::load(vfs, asset_root));
    }
    S3G_CHECK(sd::StockGameplayConfig::load(vfs, asset_root));
    S3G_TRY(auto manifest, sd::GameAssets::load(vfs, asset_root));
    S3G_TRY(auto graphs, graph_runtime::StockGraphs::load(vfs, asset_root, manifest));
    if (map != nullptr) {
        S3G_CHECK(skate_world::validate_runtime(*map));
    }
    log::eprintln(fmt::cat("SKATE_DIFFICULTY mode=", difficulty::key(difficulty),
                           " native_index=", static_cast<std::uint32_t>(difficulty)));
    std::unique_ptr<Sim> sim(new Sim());
    State& s = *sim->s_;
    s.graphs.emplace(std::move(graphs));
    S3G_TRY(auto physics, GamePhysics::load_with_difficulty(vfs, asset_root, map, difficulty));
    s.physics.emplace(std::move(physics));
    S3G_TRY(auto skater, SkaterRuntime::load(vfs, asset_root, *s.graphs, *s.physics, difficulty::key(difficulty)));
    s.skater.emplace(std::move(skater));
    S3G_TRY(auto controls, PlayerControls::load(vfs, asset_root));
    s.controls.emplace(std::move(controls));
    S3G_TRY(auto camera, camera::CameraRuntime::load(vfs, asset_root));
    s.camera.emplace(std::move(camera));
    return sim;
}

float Sim::time_step() const { return s_->physics->simref_time_step(); }

rs::Result<TickRecord, std::string> Sim::step(Pad pad) {
    State& s = *s_;
    GamePhysics& physics = *s.physics;
    SkaterRuntime& skater = *s.skater;
    // ---- PreUpdate: customiser::preferences with the default draft profile.
    // No settings/character.json and no customisation library in the pack, so the
    // draft is {"selections":{},"morphs":{}} and apply_preferences uses its defaults.
    physics.set_equipment_preferences(static_cast<float>(0.7), static_cast<float>(0.7));
    physics.set_gesture_preferences(std::nullopt);
    skater.animation.set_customisation(1, 0);
    skater.animation.motion.animation.posture.set_profile(0);

    // ---- PreUpdate: input::poll_controllers (slot 0 = our pad, 1..3 disconnected).
    XboxState state{};
    state.buttons = pad.buttons;
    state.triggers = {pad.left_trigger, pad.right_trigger};
    state.left = {pad.lx, pad.ly};
    state.right = {pad.rx, pad.ry};
    if (!s.last_pad.has_value() || *s.last_pad != state) {
        // XInput's dwPacketNumber changes only when the state changes.
        s.packet_number = s.packet_number + 1u;
        s.last_pad = state;
    }
    using input::platform::DeviceError;
    using input::platform::DevicePacket;
    using Sample = rs::Result<DevicePacket, DeviceError>;
    DevicePacket packet{};
    packet.number = s.packet_number;
    packet.state = state;
    // XINPUT_DEVSUBTYPE_GAMEPAD (also what the web path synthesizes).
    packet.subtype = 1;
    std::array<Sample, 4> samples = {
        Sample(packet),
        Sample(rs::make_err(DeviceError{DeviceError::Disconnected{}})),
        Sample(rs::make_err(DeviceError{DeviceError::Disconnected{}})),
        Sample(rs::make_err(DeviceError{DeviceError::Disconnected{}})),
    };
    // ---- PreUpdate collect + FixedUpdate SimulationSet::Input publish_actions.
    const input::PublishedTickInput published = input::host_frame(s.controller, samples);

    // ---- FixedUpdate SimulationSet::Controls: controls::sample
    physics::controls::sample(published, physics, skater, *s.camera, *s.controls);

    // ---- FixedUpdate SimulationSet::Physics: physics.rs `advance` system
    if (physics.failed) return rs::make_err(std::string("physics already failed"));
    auto actions = published._0.actions();
    const bool input_available = published._0.controller_available();
    const std::uint64_t tick = physics.ticks;
    auto result = physics::frame::advance(physics, skater, *s.controls, *s.graphs, actions, input_available,
                                          *s.camera);
    if (result.is_err()) {
        physics.failed = true;
        return rs::make_err(fmt::cat(result.unwrap_err(), "; state=", debug_name(skater.player_state.current()),
                                     "; tick=", physics.ticks));
    }

    // ---- Update: camera::present sets the window aspect ratio.
    s.camera->set_aspect_ratio(WINDOW_ASPECT);

    return observe(tick);
}

TickRecord Sim::observe(std::uint64_t tick) {
    State& s = *s_;
    const GamePhysics& physics = *s.physics;
    const SkaterRuntime& skater = *s.skater;
    const auto state = skater.player_state.current();
    const std::string motion_state =
        state_path(s.graphs->motion.binding, skater.animation.motion_controller.frame.current);
    const std::string action_state =
        state_path(s.graphs->action.binding, skater.animation.action_controller.frame.current);
    const std::string clip = skater.animation.motion.animation.current_name.value_or(std::string());
    std::vector<Body> board;
    static const std::array<std::pair<BodyId, const char*>, 7> ORDER = {{
        {BodyId::RightFrontWheel, "wheel_rf"},
        {BodyId::LeftFrontWheel, "wheel_lf"},
        {BodyId::RightBackWheel, "wheel_rb"},
        {BodyId::LeftBackWheel, "wheel_lb"},
        {BodyId::FrontTruck, "truck_f"},
        {BodyId::BackTruck, "truck_b"},
        {BodyId::Deck, "deck"},
    }};
    for (const auto& [id, name] : ORDER) {
        const auto r = physics.board.bodies()[sc::physics::board::index(id)].rates;
        Body b{};
        b.name = name;
        b.position = v3(r.position);
        b.orientation = {r.orientation.x, r.orientation.y, r.orientation.z, r.orientation.w};
        b.linear_velocity = v3(r.linear_velocity);
        b.angular_velocity = v3(r.angular_velocity);
        board.push_back(b);
    }
    const auto* output = physics.simref_output();
    std::vector<std::string> events;
    std::array<std::uint32_t, 2> contacts{0, 0};
    for (const auto& event : physics.simref_events()) {
        if (const auto* c = std::get_if<PhysicsEvent::StateChanged>(&event.value)) {
            events.push_back(fmt::cat("STATE:", debug_name(c->from), ">", debug_name(c->to)));
        } else if (std::holds_alternative<PhysicsEvent::Landing>(event.value)) {
            events.push_back("LANDING");
        } else if (std::holds_alternative<PhysicsEvent::Wipeout>(event.value)) {
            events.push_back("WIPEOUT");
        } else if (const auto* contact = std::get_if<PhysicsEvent::Contact>(&event.value)) {
            if (contact->body == PhysicsBody::Board) contacts[0] += 1;
            else contacts[1] += 1;
        }
    }
    if (contacts[0] > 0) events.push_back(fmt::cat("CONTACT_BOARD:", contacts[0]));
    if (contacts[1] > 0) events.push_back(fmt::cat("CONTACT_RIDER:", contacts[1]));
    if (motion_state != s.prev_motion) events.push_back("MOTION:" + motion_state);
    if (action_state != s.prev_action) events.push_back("ACTION:" + action_state);
    if (clip != s.prev_clip && !clip.empty()) events.push_back("CLIP:" + clip);
    const auto trick = skater.animation.motion.score_packet.trick_names.first;
    const std::string trick_name = trick ? decode_name(*trick) : std::string();
    if (trick != s.prev_trick && trick.has_value()) events.push_back("TRICK:" + trick_name);
    const std::string score_name(skater.scoring.displayed_name());
    if (score_name != s.prev_score_name && !score_name.empty()) events.push_back("SCORE:" + score_name);
    // Derived (host-side) edges: the game's PhysicsEventBuffer only ever carries
    // StateChanged, so airborne/landed are derived from the state category (id/100).
    const std::uint32_t id = static_cast<std::uint32_t>(state);
    if (s.prev_state_id / 100 != 2 && id / 100 == 2) events.push_back("AIRBORNE");
    if (s.prev_state_id / 100 == 2 && id / 100 != 2) {
        events.push_back(fmt::cat("LANDED:type", skater.landing_quality.landing_type_96));
    }
    s.prev_state_id = id;
    const bool wiping = output != nullptr && output->wiping_out;
    if (wiping && !s.prev_wiping) events.push_back("BAIL");
    const auto rider = skater.skeleton.bodies()[0].rates;
    TickRecord record{};
    record.tick = tick;
    record.physical_state = debug_name(state);
    record.physical_state_id = id;
    record.motion_state = motion_state;
    record.action_state = action_state;
    record.animation_clip = clip;
    record.board = std::move(board);
    record.rider_root_position = v3(rider.position);
    record.rider_root_velocity = v3(rider.linear_velocity);
    record.contact_count = output != nullptr ? output->contact_count : 0;
    record.grounded = output != nullptr && output->grounded;
    record.wiping_out = wiping;
    record.landed = output != nullptr && output->landed;
    record.trick_name = trick_name;
    record.score_name = score_name;
    record.events = std::move(events);
    s.prev_motion = motion_state;
    s.prev_action = action_state;
    s.prev_clip = clip;
    s.prev_trick = trick;
    s.prev_score_name = score_name;
    s.prev_wiping = wiping;
    return record;
}

}  // namespace s3::game::driver
