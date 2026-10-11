// Port of skate-game/src/input.rs.
//! Platform input adapter; no animation or physics state mutation here.
#pragma once
#include <array>
#include <utility>

#include "s3/core/input/gameplay_map.hpp"
#include "s3/core/input/tick.hpp"
#include "s3/game/input/controllers.hpp"
#include "s3/game/input/gesture_catalog.hpp"
#include "s3/game/input/gesture_input.hpp"
#include "s3/game/input/gesture_mapping.hpp"
#include "s3/game/input/gesture_mapping_data.hpp"
#include "s3/game/input/platform.hpp"

namespace s3::game::input {

using controllers::ControllerInput;
using controllers::ControllerStatus;

/// `#[derive(Clone, Copy, Debug)] struct PublishedTickInput(pub TickInput)`.
/// `impl Default`: tick 0, all-zero gameplay actions, no controller.
struct PublishedTickInput {
    ::s3::input::tick::TickInput _0 = ::s3::input::tick::TickInput::new_(
        0, ::s3::input::gameplay_map::GameplayActions::from_values(std::array<float, 18>{}), false);

    static PublishedTickInput default_() { return PublishedTickInput{}; }
};

// SIMREF: InputPlugin / poll_controllers / publish_actions ECS systems removed.
// `host_frame` reproduces, for one host frame that runs exactly one fixed tick:
//   PreUpdate  poll_controllers -> ControllerInput::collect(4 device samples)
//   FixedUpdate SimulationSet::Input publish_actions -> publish_actions(); tick_input()
// with gameplay active (no menu), so discard_gameplay() is not called.
inline PublishedTickInput host_frame(ControllerInput& input,
                                     std::array<platform::DeviceResult, ::s3::input::history::DEVICE_SLOTS> samples) {
    input.collect(std::move(samples));
    input.publish_actions();
    return PublishedTickInput{input.tick_input()};
}

}  // namespace s3::game::input
