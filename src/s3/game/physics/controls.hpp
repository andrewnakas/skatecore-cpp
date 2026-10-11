// Port of skate-game/src/physics/controls.rs.
//! Controller-derived state is evaluated on the same fixed gameplay tick.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/graph/intents.hpp"
#include "s3/core/input/controller.hpp"
#include "s3/core/input/riding_intentions.hpp"
#include "s3/game/common.hpp"
#include "s3/game/input.hpp"
#include "s3/game/input/gesture_input.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::physics::controls {

using sc::input::controller::ActionMap;

/// Borrow the existing action packet; override only the native left-stick pair.
struct SimulationActions final : ActionMap {
    ActionMap* source = nullptr;
    std::optional<std::array<float, 2>> offboard_axes;

    SimulationActions(ActionMap& source_, std::optional<std::array<float, 2>> axes)
        : source(&source_), offboard_axes(axes) {}
    float value(std::uint32_t action) override;
    std::uint8_t state(std::uint32_t action) override;
};

struct PlayerControls {
    sc::input::controller::DerivedControllerInput controller = initial_controller();
    std::optional<std::array<float, 4>> offboard_direction;
    std::vector<sc::input::riding_intentions::RidingIntent> intents;
    sc::graph::intents::IntentMap action_intents{};
    std::uint64_t ticks = 0;
    std::uint32_t actor_flags = 0;
    bool bumper_state_502 = false;
    bool bumper_state_104 = false;
    sc::input::riding_intentions::PushPreferences preferences{};
    // One tick's native PlayerUI82898D20 result, shared by animation and PhysIn.
    // None means the native offboard remap gate did not run, not missing camera.
    std::optional<std::array<float, 2>> offboard_axes;
    std::optional<input::gesture_input::GestureInput> gestures;

    static sc::input::controller::DerivedControllerInput initial_controller();

    /// PlayerUI82898920 transforms the gameplay packet before Raw/Derived input.
    /// Only its offboard branch is enabled here; onboard behavior is unchanged.
    Status update_for_physics(ActionMap& map, const GamePhysics& physics, const SkaterRuntime& skater,
                              const camera::CameraRuntime& camera);
    /// Parent must pass this SAME sampled mapping into input_phase::advance.
    /// Do not rotate again: the camera/state may have advanced since sampling.
    SimulationActions simulation_actions(ActionMap& source) const;
    static rs::Result<PlayerControls, std::string> load(const sd::Vfs& vfs, const std::string& root);
    void publish_gestures(std::uint32_t difficulty, std::uint32_t physical_state);
    void update(ActionMap& map, float dt, float magnitude_threshold, std::uint32_t physical_capabilities);
};

// SIMREF: was the Bevy FixedUpdate system (SimulationSet::Controls); now a plain fn.
void sample(const input::PublishedTickInput& input, const GamePhysics& physics, const SkaterRuntime& skater,
            const camera::CameraRuntime& camera, PlayerControls& player);

/// Complete numerical remap82898D20 (private in Rust).
std::array<float, 2> camera_relative_axes(std::array<float, 2> stick, std::array<std::array<float, 3>, 3> camera);
std::array<float, 3> cross_camera_axis(std::array<float, 3> a, std::array<float, 3> b);
std::array<float, 3> normalize_camera_axis(std::array<float, 3> v);

}  // namespace s3::game::physics::controls
