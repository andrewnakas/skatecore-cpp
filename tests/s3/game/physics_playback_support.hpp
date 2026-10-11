// Shared scaffolding for the ported production-playback tests (skate-game/src/tests/*_playback.rs).
// Rust reads SKATE3_ASSET_ROOT; here the cleanroom pack (s3test::game::assets(), root "assets").
// Every helper is a direct port of a pattern repeated verbatim in the Rust test files.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "game_test_assets.hpp"
#include "harness.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/gesture_patterns.hpp"
#include "s3/data/manifest.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/common.hpp"
#include "s3/game/graph_runtime.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/controls.hpp"
#include "s3/game/physics/frame.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3test::game::playback {

/// `GameAssets::load(root)` + `StockGraphs::load(root, &assets)`. Loaded once per process: the
/// graphs are immutable inputs of `frame::advance`, so sharing them does not change a replay.
struct Stock {
    const s3::data::Vfs* vfs = nullptr;
    std::string root;
    s3::game::graph_runtime::StockGraphs graphs;
};

inline const Stock* stock() {
    static std::optional<Stock> loaded;
    static bool tried = false;
    if (!tried) {
        tried = true;
        const auto* vfs = assets();
        if (vfs == nullptr) return nullptr;
        auto manifest = s3::data::GameAssets::load(*vfs, ASSET_ROOT);
        if (!manifest.ok) throw std::runtime_error("GameAssets::load: " + manifest.error);
        auto graphs = s3::game::graph_runtime::StockGraphs::load(*vfs, ASSET_ROOT, manifest.value);
        if (!graphs.is_ok()) throw std::runtime_error("StockGraphs::load: " + graphs.unwrap_err());
        loaded.emplace(Stock{vfs, ASSET_ROOT, std::move(graphs.unwrap())});
    }
    return loaded ? &*loaded : nullptr;
}

/// Rust `.unwrap()` on a `Result<T, String>`, panicking with the error text.
template <class T>
T unwrap(s3::rs::Result<T, std::string> r, const std::string& context) {
    if (!r.is_ok()) throw std::runtime_error(context + ": " + r.unwrap_err());
    return std::move(r.unwrap());
}

[[noreturn]] inline void panic(const std::string& message) { throw std::runtime_error(message); }

/// `XboxState { buttons, triggers, left, right }`.
inline s3::input::xbox::XboxState xbox(std::uint16_t buttons, std::array<std::uint8_t, 2> triggers,
                                       std::array<std::int16_t, 2> left, std::array<std::int16_t, 2> right) {
    s3::input::xbox::XboxState state{};
    state.buttons = buttons;
    state.triggers = triggers;
    state.left = left;
    state.right = right;
    return state;
}

/// GameInputManager 82696030 negates the mapped Y before matching:
/// `|p| [(p[0] * 32767.) as i16, (-p[1] * 32767.) as i16]`.
inline std::array<std::int16_t, 2> sample(std::array<float, 2> p) {
    return {s3::rs::as_int<std::int16_t>(p[0] * 32767.0f), s3::rs::as_int<std::int16_t>(-p[1] * 32767.0f)};
}

/// `skate_data::gesture_patterns::load(&root.join("private/stock/data/joystick").join(file)).unwrap()`.
inline std::vector<s3::data::Pattern> patterns(const Stock& s, const std::string& file) {
    const auto path = s3::data::join_path(s3::data::join_path(s.root, "private/stock/data/joystick"), file);
    auto loaded = s3::data::gesture_patterns::load(*s.vfs, path);
    if (!loaded.ok) throw std::runtime_error(path + ": " + loaded.error);
    return std::move(loaded.value);
}

inline bool eq_ignore_ascii_case(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; };
        if (lower(a[i]) != lower(b[i])) return false;
    }
    return true;
}

/// One production tick as every playback test runs it: raw packet -> PlayerControls::update ->
/// (optional) publish_gestures -> frame::advance.
inline s3::game::Status tick(s3::game::physics::GamePhysics& physics, s3::game::physics::SkaterRuntime& skater,
                             s3::game::physics::PlayerControls& controls,
                             s3::game::input::controllers::ControllerInput& input,
                             s3::game::camera::CameraRuntime& camera, const Stock& s,
                             const s3::input::xbox::XboxState& state, bool publish_gestures) {
    input.sample_raw_for_test(state);
    auto actions = input.player_actions();
    controls.update(actions, physics.settings.step.simulation.time_step, physics.settings.input_magnitude_threshold,
                    skater.player_input.physical.scoring.capabilities_204);
    if (publish_gestures) {
        controls.publish_gestures(physics.animation_profile.physics_mode,
                                  skater.player_input.physical.state.state_16);
    }
    return s3::game::physics::frame::advance(physics, skater, controls, s.graphs, actions, true, camera);
}

/// `{:?}` of a `Vec<String>`.
inline std::string debug_strings(const std::vector<std::string>& v) {
    std::ostringstream os;
    os << "[";
    for (std::size_t i = 0; i < v.size(); ++i) os << (i ? ", " : "") << '"' << v[i] << '"';
    os << "]";
    return os.str();
}

inline std::string hex8(std::uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%08x", v);
    return buf;
}

/// The rolling approach every scoop test applies at tick 60: `linear_velocity = (0, 0, 8)` on every
/// board and skeleton body.
inline void roll(s3::game::physics::GamePhysics& physics, s3::game::physics::SkaterRuntime& skater) {
    for (auto& body : physics.board.bodies_mut()) body.rates.linear_velocity = s3::math::Vector3::new_(0.0f, 0.0f, 8.0f);
    for (auto& body : skater.skeleton.bodies_mut())
        body.rates.linear_velocity = s3::math::Vector3::new_(0.0f, 0.0f, 8.0f);
}

/// The takeoff boost at tick 123: `linear_velocity.y += boost` on every board and skeleton body.
inline void boost(s3::game::physics::GamePhysics& physics, s3::game::physics::SkaterRuntime& skater, float boost) {
    for (auto& body : physics.board.bodies_mut()) body.rates.linear_velocity.y += boost;
    for (auto& body : skater.skeleton.bodies_mut()) body.rates.linear_velocity.y += boost;
}

}  // namespace s3test::game::playback
