// Port of skate-core/src/player/wipeout/data.rs (whole file).
// Actual observation values consumed by Wipeout82D8F9E0/82D90358.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::player::wipeout::data {

using physics::skeleton_animation_record::AnimationPartTransform;
using point_graph::PointGraph;
using V = std::array<float, 4>;

struct Frame {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2472 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    std::uint32_t category = 0;
    float timestep = 0.0f;
    float time_on_ground = 0.0f;
    float speed = 0.0f;
    V animation_up{};
    float landing_angle = 0.0f;
    V deck_velocity{};
    V com_velocity{};
    std::int32_t jump_fix_frames = 0;
    /// Actual solver deck pose and the retained Processed0/16/32 basis.
    AnimationPartTransform deck{};
    AnimationPartTransform input_board{};
    AnimationPartTransform world_to_animation{};
    V closing_velocity{};
    std::uint32_t board_material_flags = 0;
    bool board_contact = false;
    bool wheel_contact = false;
    V board_contact_normal{};
    float opposing_contact = 0.0f;
    std::array<float, 8> regions_force{};
    float maximum_skater_force = 0.0f;
    float vehicle_force = 0.0f;
    bool group_8 = false;
    bool conflicting = false;
    bool compliant = false;
    V highest_normal{};
    V pose_error{};
    float maximum_pose_error = 0.0f;
    bool flip_active = false;
    float flip_requested_speed = 0.0f;
    float system_up_y = 0.0f;
    bool grind_selected = false;
    bool grind_normal_valid = false;
    V grind_normal{};
};

struct Mode {
    bool check_squash = false;
    bool check_bad_landing = false;
    float ground_xz = 0.0f;
    float bad_landing_scale = 0.0f;
};

struct GroundSettings {
    float vehicle_scalar = 0.0f;
    float vehicle_contact = 0.0f;
    float skitch_contact = 0.0f;
    float skitch_scalar = 0.0f;
    float skitch_arms_scalar = 0.0f;
    float skater_scalar = 0.0f;
    float max_squash = 0.0f;
    float max_squash_coffin = 0.0f;
    float max_displacement = 0.0f;
    float max_contact = 0.0f;
    float max_arm_contact = 0.0f;
    float max_deck_error = 0.0f;
    float opposing_contact = 0.0f;
    float y_acceleration = 0.0f;
    float light_dmo_scalar = 0.0f;
    float player_scalar = 0.0f;
    float ai_scalar = 0.0f;
    float skitch_acc_scalar = 0.0f;
    float balance_total = 0.0f;
    float balance_min_speed = 0.0f;
    float balance_base = 0.0f;
};

struct AirSettings {
    float xz_trick = 0.0f;
    float y_trick = 0.0f;
    float xz_acceleration = 0.0f;
    float y_acceleration = 0.0f;
    float max_squash = 0.0f;
    float max_displacement = 0.0f;
    float max_contact = 0.0f;
    float max_arm_contact = 0.0f;
    float body_flip_scalar = 0.0f;
    float body_flip_acc_scalar = 0.0f;
    float light_dmo_scalar = 0.0f;
    std::int32_t ignore_danger_frames = 0;
    float max_landing_speed = 0.0f;
    float max_stairs_speed = 0.0f;
    float max_grind_speed = 0.0f;
    /// Layout0 PointNegGraph; original directly reads X16/Y48.
    PointGraph<8> max_landing_angle{};
};

struct Settings {
    GroundSettings ground;
    AirSettings air;
    float lean_contact_y = 0.0f;
};

struct RequestInput {
    std::uint32_t flags_2468 = 0;
    std::uint32_t flags_2476 = 0;
    std::uint32_t flags_2480 = 0;
    std::uint32_t flags_2484 = 0;
    float animation_up_y = 0.0f;
    std::uint32_t category = 0;
};

}  // namespace s3::player::wipeout::data

namespace s3::player::wipeout {
// pub use data::{AirSettings, Frame, GroundSettings, Mode, RequestInput, Settings, V};
using data::AirSettings;
using data::Frame;
using data::GroundSettings;
using data::Mode;
using data::RequestInput;
using data::Settings;
using data::V;
}  // namespace s3::player::wipeout
