// Port of simref crates/sim/src/driver.rs (SIMREF addition): headless host loop that
// replaces the Bevy App schedule. Startup mirrors skate-game/src/main.rs `main()` (native path):
//   StockGameplayConfig::load -> GameAssets::load -> StockGraphs::load ->
//   skate_world::validate_runtime(map) -> GamePhysics::load_with_difficulty ->
//   SkaterRuntime::load -> PlayerControls::load, and CameraRuntime::load.
// Each `step` is one host frame that runs exactly one FixedUpdate tick.
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/input/xbox.hpp"
#include "s3/data/skate_map.hpp"
#include "s3/data/vfs.hpp"
#include "s3/game/common.hpp"
#include "s3/game/physics/fwd.hpp"

namespace s3::game::input::controllers {
struct ControllerInput;
}

namespace s3::game::driver {

/// Default window in skate-game/src/app.rs: `resolution: (1280, 800)`.
inline constexpr float WINDOW_ASPECT = 1280.0f / 800.0f;

/// One controller sample in XInput units (what XInputGetState returns).
struct Pad {
    std::uint16_t buttons = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
    std::int16_t lx = 0;
    std::int16_t ly = 0;
    std::int16_t rx = 0;
    std::int16_t ry = 0;
};

struct Body {
    const char* name = "";
    std::array<float, 3> position{};
    std::array<float, 4> orientation{};
    std::array<float, 3> linear_velocity{};
    std::array<float, 3> angular_velocity{};
};

struct TickRecord {
    std::uint64_t tick = 0;
    std::string physical_state;
    std::uint32_t physical_state_id = 0;
    std::string motion_state;
    std::string action_state;
    std::string animation_clip;
    std::vector<Body> board;
    std::array<float, 3> rider_root_position{};
    std::array<float, 3> rider_root_velocity{};
    std::uint32_t contact_count = 0;
    bool grounded = false;
    bool wiping_out = false;
    bool landed = false;
    std::string trick_name;
    std::string score_name;
    std::vector<std::string> events;
};

/// Render-side view of the completed tick (web host; not part of the trace contract).
/// Mirrors what presentation.rs `capture` snapshots plus the audio observation inputs.
struct Presentation {
    /// skater.animated_skeleton.roots.animation_to_world (native column-major 4x4).
    std::array<std::array<float, 4>, 4> root{};
    /// skater.render_pose: global bone matrices in animation space.
    const std::vector<std::array<std::array<float, 4>, 4>>* bones = nullptr;
    std::uint64_t pose_generation = 0;
    bool has_camera = false;
    std::array<float, 3> camera_position{};
    /// CameraFrame basis columns: right, up, at.
    std::array<std::array<float, 3>, 3> camera_basis{};
    float field_of_view_degrees = 0.0f;
    bool camera_discontinuity = false;
    /// processed.flags_2472 bit 10 (teleport): presentation snaps instead of blending.
    bool teleport = false;
    /// physics.clock.period() in seconds (the fixed-update cadence).
    double period_seconds = 0.0;
    // Audio observation (physics/audio_observation.rs publish).
    std::uint32_t wheel_surface = 0;
    std::uint32_t grind_audio_surface = 0;
    bool powersliding = false;
    float board_speed = 0.0f;
    float footstep_strength = 0.0f;
    float foot_push_speed = 0.0f;
    std::string trick_identifier;
    // Scoring (HUD).
    float sequence_score = 0.0f;
    float multiplier = 1.0f;
    float line_score = 0.0f;
    float total_score = 0.0f;
};

std::string decode_name(const sc::animation::output::attributes::AttributeName& name);

class Sim {
public:
    /// `asset_root` is the game's `--assets` directory inside `vfs` ("assets" for the pack);
    /// `map` the parsed .skate (nullptr: test world). `difficulty`: nullptr -> Difficulty::load.
    static rs::Result<std::unique_ptr<Sim>, std::string> load(const sd::Vfs& vfs, const std::string& asset_root,
                                                              const sd::SkateMap* map, const char* difficulty);
    ~Sim();
    float time_step() const;
    /// One host frame / one fixed tick. Returns the post-tick observation.
    rs::Result<TickRecord, std::string> step(Pad pad);
    /// Window aspect ratio camera::present applies after each tick (default WINDOW_ASPECT, as simref).
    void set_aspect_ratio(float aspect);
    /// Render/audio view of the last completed tick.
    Presentation presentation() const;

private:
    Sim();
    TickRecord observe(std::uint64_t tick);
    struct State;
    std::unique_ptr<State> s_;
};

}  // namespace s3::game::driver
