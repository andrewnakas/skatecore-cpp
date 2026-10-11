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

private:
    Sim();
    TickRecord observe(std::uint64_t tick);
    struct State;
    std::unique_ptr<State> s_;
};

}  // namespace s3::game::driver
