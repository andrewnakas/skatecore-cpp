// Port of skate-core/src/player/offboard/board_possession.rs and the State methods /
// types of its lifecycle.rs and motion.rs submodules.
// Original TU3 SkateboardController. Lifecycle state448 remains in the existing
// SkateboardControllerFields; this module owns only its additional data.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/player/lifecycle.hpp"
#include "s3/core/player/offboard/board_possession/angular.hpp"
#include "s3/core/player/offboard/board_possession/manager.hpp"
#include "s3/core/player/offboard/board_possession/math.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::offboard::board_possession {

using Vector = std::array<float, 4>;
using Frame = std::array<Vector, 4>;
using SkateboardControllerFields = ::s3::player::lifecycle::SkateboardControllerFields;
using point_graph::PointGraph;
inline constexpr Frame IDENTITY{{{1.0f, 0.0f, 0.0f, 0.0f},
                                 {0.0f, 1.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 1.0f, 0.0f},
                                 {0.0f, 0.0f, 0.0f, 0.0f}}};

struct Retrieval {
    Frame initial_0 = IDENTITY;
    Frame target_64 = IDENTITY;
    Frame current_128 = IDENTITY;
    float elapsed_192 = 0.0f;
    float duration_196 = 0.0f;
    float progress_200 = 0.0f;
    float weight_204 = 0.0f;
};

struct HandDrive {
    /// Native child frame224/288, parent frame256/320.
    Frame child = IDENTITY;
    Frame parent = IDENTITY;
    /// Linear then angular spring/maxvelocity,damping,strength,type.
    std::array<std::array<std::uint32_t, 4>, 2> dynamics{{{0, 0, 0, 2}, {0, 0, 0, 2}}};
};

struct Processed {
    Frame board_frame_64{};
    Frame player_frame_192{};
    Vector position_592{};
    Vector velocity_912{};
    Vector direction_400{};
    Vector hide_direction_464{};
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2488 = 0;
};

struct Fill {
    float angle_36 = 0.0f;
    float angle_40 = 0.0f;
    bool held_311 = false;
    bool free_312 = false;
    bool returning_313 = false;
    bool hiding_321 = false;
    bool flag_322 = false;
    bool flag_323 = false;
    bool flag_324 = false;
};

// ---- lifecycle.rs types ----
struct Settings {
    float hide_distance = 0.0f;
    float hide_offset = 0.0f;
    float return_distance = 0.0f;
    float mounted_return_distance = 0.0f;
    float mounting_time = 0.0f;
    PointGraph<8> retrieval_time{};
    PointGraph<8> retrieval_weight{};
    float throw_pitch = 0.0f;
    PointGraph<8> throw_velocity{};
    float throw_target_pitch = 0.0f;
    float throw_pitch_scalar = 0.0f;
    float throw_roll_scalar = 0.0f;
    float throw_yaw_scalar = 0.0f;
};

/// Actual completed physical and evaluated skeleton observations.
struct Observation {
    Processed processed{};
    std::uint32_t board_collision_flags_872 = 0;
    std::uint32_t board_state_840 = 0;
    std::array<bool, 2> hand_contacts{};
    std::array<Vector, 2> physical_hand_positions{};
    Frame animation_board_frame_12624{};
    std::array<Frame, 2> animation_hand_frames{};
    Frame attachment_frame_0{};
};

struct Alignment {
    Vector first_1008{};
    Vector second_1024{};
    float factor_1040 = 0.0f;
    bool flag_1044 = false;
};

/// Each call mutates the actual board/solver owner. No observation defaults.
/// (Rust trait -> abstract interface.)
struct Effects {
    virtual ~Effects() = default;
    virtual void enable_animation_soft() = 0;
    virtual void enable_animation_angular_only() = 0;
    virtual void disable_animation() = 0;
    virtual void disable_linear_drive() = 0;
    ///82C03AF8 clears8384 bit7, executes091F8/090C0, enables selected volumes.
    virtual void standard_board() = 0;
    ///7549C..54D0 sets8384 bit7, deck drag literal416FFFFF,09160, enables volumes.
    virtual void released_board() = 0;
    virtual void collision_volumes(bool enabled) = 0;
    virtual void clear_alignment() = 0;
    virtual void alignment(Alignment value) = 0;
    virtual void velocity(Vector value) = 0;
    virtual void position(Vector value) = 0;
    virtual void hook_frame(Frame value) = 0;
    virtual void target_position_velocity(Vector value) = 0;
    virtual void torque(Vector value) = 0;
};

struct State {
    Retrieval retrieval{};
    std::array<HandDrive, 2> hands{};
    std::uint32_t selected_hand_424 = 2;

    // ---- lifecycle.rs ----
    ///75EA0 retains the optional throw's word444 write made inside LetGo.
    void stop(SkateboardControllerFields& f, const Observation& o, const Settings& s, Effects& e);
    ///75370 writes held state BEFORE selecting/arming the hand and drive frames.
    void hold(SkateboardControllerFields& fields, const Observation& o, Effects& e);
    void disable_hand();
    ///75440 does not itself change448. Caller transitions after all actions.
    void let_go(SkateboardControllerFields& fields, const Observation& o, const Settings& s, Effects& e);
    void hide(const Observation& o, Effects& e);
    void retrieve(const SkateboardControllerFields& fields, const Observation& o, const Settings& s, Effects& e);
    ///75F00. Fields are borrowed from the single existing controller owner.
    void update_state(SkateboardControllerFields& f, const Observation& o, const Settings& s, Effects& e);

    // ---- motion.rs ----
    ///751D8: inverse board rotation and inverse translation are evaluated
    ///separately before composing the selected hand. Preserve z/y/x order.
    void update_drive_frames(const Observation& o);
    /// Controller block82DB6150..61B0, after physical state's update callback.
    void update(SkateboardControllerFields& f, const Observation& o, const Settings& s, Effects& e);
};

/// Complete82D76D20 numerical/byte publication, given actual82BE3170 bone11.
Fill fill(const SkateboardControllerFields& fields, const State& state, const Processed& p, Frame bone11);

namespace motion {
///764F0 retains even the initial zero-yaw construction and all fused products.
Vector throw_velocity(const Processed& p, const Settings& s);
///767D0: yaw, pitch, roll torque outputs in source append order.
std::array<Vector, 3> throw_torques(const Processed& p, const Settings& s);
}  // namespace motion

}  // namespace s3::player::offboard::board_possession
