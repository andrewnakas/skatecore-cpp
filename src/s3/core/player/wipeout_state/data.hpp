// Port of skate-core/src/player/wipeout_state/data.rs (whole file).
// Original WipeoutGround Reset82D3B3A8. Native offsets document ownership.
// The `impl State` blocks of output.rs and recovery.rs are member functions
// declared here and defined in output.cpp / recovery.cpp.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/rs.hpp"

namespace s3::player::wipeout_state {
namespace output {
struct Output;
}
namespace recovery {
struct Settings;
}
}  // namespace s3::player::wipeout_state

namespace s3::player::wipeout_state::data {

struct State {
    float time = 0.0f;                    // 304
    float settled_time = 0.0f;            // 308
    float time_until_teleport = 0.0f;     // 312
    float impaled_time = 0.0f;            // 316
    float no_support_time = 0.0f;         // 320
    float response_time = 0.0f;           // 324
    float extra_weight_zero_time = 0.0f;  // 328
    float slow_time = 0.0f;               // 332
    float surface_height = 0.0f;          // 336
    std::array<float, 4> board_offset{};        // 352
    std::array<float, 4> forward{};             // 368
    std::array<float, 4> right{};               // 384
    std::array<float, 4> angular_velocity{};    // 400
    std::array<float, 4> retained_tilt{};       // 416
    std::array<float, 4> velocity{};            // 432
    std::array<float, 4> retained_velocity{};   // 448
    std::array<float, 4> predicted_position{};  // 464
    bool over = false;                      // 480
    bool slow = false;                      // 481
    bool material_eleven_response = false;  // 482
    bool air_collision_mode = false;        // 483
    bool request_teleport = false;          // 484
    bool move_board = false;                // 485
    bool special_surface = false;           // 486
    bool below_surface = false;             // 487
    bool retained_velocity_active = false;  // 488
    float sideways_input = 0.0f;            // 492
    float forward_input = 0.0f;             // 496
    float orientation = 0.0f;               // 500
    float collision_weight = 0.0f;          // 504
    float controlled_weight = 0.0f;         // 508
    float control_time = 0.0f;              // 512
    float retained_sideways_input = 0.0f;   // 516
    float retained_forward_input = 0.0f;    // 520
    float response_scalar = 0.0f;           // 524
    float extra_weight = 1.0f;              // 528
    float maximum_speed = 10.0f;            // 532
    float response_start_speed = 0.0f;      // 536
    float response_change = 0.0f;           // 540
    std::uint32_t board_move_frames = 0;    // 544, initialized by Enter
    std::uint32_t response_count = 0;       // 548
    std::int32_t response_frames = -1;      // 552
    std::int32_t airborne_frames = -1;      // 556
    std::int32_t teleport_countdown = -1;   // 560
    bool prevent_manual = false;            // 564
    bool ignore_reset = false;              // 565
    bool reset_ever = false;                // 566
    bool ever_settled = false;              // 567
    bool recovery_eligible = false;         // 568
    bool teleport_pending = false;          // 569
    bool ever_impaled = false;              // 570
    bool direction_initialized = false;     // 571
    bool material_ten_response = false;     // 572
    bool imminent_surface_twelve = false;   // 573
    bool allow_retained_velocity = true;    // 574
    bool response_finished = false;         // 575
    bool surface_query = false;             // 576
    std::size_t profile = 0;                // 580
    float predicted_time = rs::f32::MAX;    // 832

    static State default_() { return State{}; }

    // ---- output.rs (impl State) ----
    output::Output output(const physics::skeleton_animation_record::AnimationPartTransform& hips,
                          std::array<float, 4> normal) const;

    // ---- recovery.rs (impl State) ----
    float response_strength() const;
    void manage_recovery(std::uint32_t flags2468, std::uint32_t flags2472, std::uint32_t flags2484, float timestep,
                         const recovery::Settings& settings);
    void post_physics(std::array<float, 4> hips_velocity, std::array<float, 4> neck_velocity, bool support_contact,
                      const recovery::Settings& settings);

private:
    bool should_teleport(std::uint32_t flags2472, std::uint32_t flags2484, float timestep,
                         const recovery::Settings& settings);
};

}  // namespace s3::player::wipeout_state::data

namespace s3::player::wipeout_state {
using data::State;
}
