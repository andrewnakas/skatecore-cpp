// Port of skate-game/src/physics/controls.rs.
#include "s3/game/physics/controls.hpp"

#include <stdexcept>

#include "s3/core/input/anticipation_intentions.hpp"
#include "s3/core/input/manual_intentions.hpp"
#include "s3/core/input/trick_intentions.hpp"
#include "s3/core/input/wipeout_intentions.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::controls {

using sc::input::controller::DerivedControllerInput;
using sc::input::controller::MagnitudeHeldSettings;
using sc::input::riding_intentions::RidingIntent;

DerivedControllerInput PlayerControls::initial_controller() {
    auto controller = DerivedControllerInput::from_words({});
    controller.initialize();
    return controller;
}

void sample(const input::PublishedTickInput& input, const GamePhysics& physics, const SkaterRuntime& skater,
            const camera::CameraRuntime& camera, PlayerControls& player) {
    auto map = input._0.actions();
    auto result = player.update_for_physics(map, physics, skater, camera);
    if (result.is_err()) {
        throw rs::Panic("Offboard controller publication: " + result.unwrap_err());
    }
    player.publish_gestures(physics.animation_profile.physics_mode, skater.player_input.physical.state.state_16);
}

Status PlayerControls::update_for_physics(ActionMap& map, const GamePhysics& physics, const SkaterRuntime& skater,
                                          const camera::CameraRuntime& camera) {
    const auto& physical = skater.player_input.physical;
    //82DB7678/7694 writes category and State75 from the same category==500.
    //82898B80..BC4 excludes grabbing an object (304) and state503.
    const bool remap =
        physical.state.category_12 == 500 && physical.off_board.flag_304 == 0 && physical.state.state_16 != 503;
    std::optional<std::array<float, 2>> axes;
    if (remap) {
        if (!camera.frame.has_value()) {
            return rs::make_err(std::string("Native offboard input requires a completed presentation camera frame"));
        }
        const float v64 = map.value(64);
        const float v65 = map.value(65);
        axes = camera_relative_axes({v64, v65}, camera.frame->basis.columns);
    }
    SimulationActions actions(map, axes);
    update(actions, physics.settings.step.simulation.time_step, physics.settings.input_magnitude_threshold,
           physical.scoring.capabilities_204);
    offboard_axes = axes;
    if (axes) {
        offboard_direction = std::array<float, 4>{(*axes)[0], 0.0f, (*axes)[1], 0.0f};
    } else {
        offboard_direction.reset();
    }
    return ok();
}

SimulationActions PlayerControls::simulation_actions(ActionMap& source) const {
    return SimulationActions(source, offboard_axes);
}

rs::Result<PlayerControls, std::string> PlayerControls::load(const sd::Vfs& vfs, const std::string& root) {
    PlayerControls out{};
    S3G_TRY(auto gestures, input::gesture_input::GestureInput::load(vfs, root));
    out.gestures = std::move(gestures);
    return out;
}

void PlayerControls::publish_gestures(std::uint32_t difficulty, std::uint32_t physical_state) {
    if (gestures.has_value()) {
        const auto& words = controller.words();
        const std::array<std::array<float, 2>, 2> axes = {
            std::array<float, 2>{rs::from_bits(words[7]), rs::from_bits(words[8])},
            std::array<float, 2>{rs::from_bits(words[9]), rs::from_bits(words[10])},
        };
        gestures->publish(axes, difficulty, actor_flags, physical_state, action_intents);
    }
}

void PlayerControls::update(ActionMap& map, float dt, float magnitude_threshold, std::uint32_t physical_capabilities) {
    offboard_axes.reset();
    MagnitudeHeldSettings held{};
    held.attribute = magnitude_threshold;
    // The validated field is present, so no engine missing-field path runs.
    held.missing_attribute_value = 0.0f;
    controller.update(map, dt, bumper_state_502, bumper_state_104, held);
    intents = sc::input::riding_intentions::produce(controller, actor_flags, preferences);
    for (const auto& i : sc::input::manual_intentions::produce(controller, actor_flags)) intents.push_back(i);
    for (const auto& i : sc::input::wipeout_intentions::produce(controller, actor_flags, physical_capabilities))
        intents.push_back(i);
    for (const auto& i : sc::input::anticipation_intentions::produce(controller)) intents.push_back(i);
    for (const auto& intent : sc::input::trick_intentions::produce(controller)) {
        intents.push_back(RidingIntent{intent.name, intent.value});
    }
    //GenerateActionGraphIntents82594310 clears the AG map through82BC1B68
    //before Listener::Fill. MG lifecycle intents use a different persistent map.
    action_intents.clear();
    for (const auto& intent : intents) action_intents.insert(intent.name, intent.value);
    ticks += 1;
}

float SimulationActions::value(std::uint32_t action) {
    if (offboard_axes.has_value()) {
        switch (action) {
            case 64: return (*offboard_axes)[0];
            case 65: return (*offboard_axes)[1];
            default: break;
        }
    }
    return source->value(action);
}

std::uint8_t SimulationActions::state(std::uint32_t action) {
    if (offboard_axes.has_value() && (action == 64 || action == 65)) {
        return value(action) != 0.0f ? 1 : 0;
    }
    return source->state(action);
}

std::array<float, 2> camera_relative_axes(std::array<float, 2> stick, std::array<std::array<float, 3>, 3> camera) {
    auto right = camera[0];
    auto up = camera[1];
    auto forward = camera[2];
    //82898D98..DB4: ABS(dot(world_up, camera_Z)) < literal820ED5E8.
    //Near either vertical pole, native retains ALL original camera axes.
    if (rs::abs(forward[1]) < rs::from_bits(0x3f7d70a4u)) {
        right = normalize_camera_axis(cross_camera_axis({0.0f, 1.0f, 0.0f}, forward));
        forward = normalize_camera_axis(cross_camera_axis(right, {0.0f, 1.0f, 0.0f}));
        up = normalize_camera_axis(cross_camera_axis(forward, right));
    }
    //CacheLine includes its count at0.82898EE0..F0C reads Buttons17-16
    //and Buttons18-19: [-LeftAnalogStickLR, 0, LeftAnalogStickUD].
    const std::array<float, 3> local = {-stick[0], 0.0f, stick[1]};
    std::array<float, 3> world{};
    for (std::size_t i = 0; i < 3; ++i) {
        world[i] = rs::mul_add(forward[i], local[2], rs::mul_add(up[i], local[1], right[i] * local[0]));
    }
    //82898F44..FAC splits world X/Z into positive/negative pad channels;
    //stock actions64/65 subtract those channels again. Keep that ordering.
    const float negative_x = world[0] >= 0.0f ? 0.0f : -world[0];
    const float positive_x = world[0] > 0.0f ? world[0] : 0.0f;
    const float negative_z = world[2] >= 0.0f ? 0.0f : -world[2];
    const float positive_z = world[2] <= 0.0f ? 0.0f : world[2];
    return {positive_x - negative_x, positive_z - negative_z};
}

std::array<float, 3> cross_camera_axis(std::array<float, 3> a, std::array<float, 3> b) {
    return {
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
    };
}

std::array<float, 3> normalize_camera_axis(std::array<float, 3> v) {
    const float squared = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    //Independent PC rsqrt seed, retaining both native refinement iterations.
    //No fitted epsilon/fallback and no claim of bit-exact Xenon emulation.
    float inverse = rs::recip(rs::sqrt(squared));
    for (int i = 0; i < 2; ++i) {
        const float correction = rs::mul_add(-squared, inverse * inverse, 1.0f);
        inverse = rs::mul_add(inverse * 0.5f, correction, inverse);
    }
    return {v[0] * inverse, v[1] * inverse, v[2] * inverse};
}

}  // namespace s3::game::physics::controls
