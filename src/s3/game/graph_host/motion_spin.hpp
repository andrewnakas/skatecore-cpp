// Port of skate-game/src/graph_host/motion_spin.rs (module `motion_spin`).
//! BodySpin82BAB640, including its native five-state channel controller.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"

namespace s3::game::graph_host::motion_animation {
class MotionAnimation;
}

namespace s3::game::graph_host::motion_spin {

using motion_animation::MotionAnimation;
using s3::point_graph::PointGraph;

/// Fields private in Rust.
struct Settings {
    PointGraph<8> map;
    float blend_out = 0.0f;
    float blend_in = 0.0f;
    float maximum_acceleration = 0.0f;
    float maximum_delta = 0.0f;
    float final_height = 0.0f;
    float height_velocity = 0.0f;
    float on_deck_height = 0.0f;
    float override_x = 0.0f;
    float override_velocity_y = 0.0f;
    PointGraph<8> landing_distance;

    static rs::Result<Settings, std::string> load(const s3::data::Collections& data);
};

///Actual publication fields used only by the native airborne prelanding query.
struct PrelandingPhysical {
    bool air_444 = false;
    float air_normal_144_y = 0.0f;
    float animation_16_x = 0.0f;
    float com_velocity_y = 0.0f;
    bool offboard_316 = false;
    bool offboard_319 = false;
    float offboard_time_32 = 0.0f;
    bool air_437 = false;
    float air_normal_36 = 0.0f;
    float air_remaining_184 = 0.0f;
    float animation_height_72 = 0.0f;

    ///825954C0 ->82595670, original source guards and strict comparisons. (`pub(crate)`)
    bool override_prelanding(const Settings& s) const;
    /// `pub(crate)`.
    bool near_landing(const Settings& s) const;
};

///82BABE20 initializes every represented scalar and the state enum to zero.
/// (Fields private in Rust.)
struct State {
    std::uint32_t mode = 0;
    float previous_spin = 0.0f;
    float back = 0.0f;
    float back_delta = 0.0f;
    float front = 0.0f;
    float front_delta = 0.0f;

    /// `prelanding`: required only on the airborne branch.
    rs::Result<rs::Unit, std::string> update(MotionAnimation& animation, std::uint32_t category,
                                             std::uint32_t physical_state, bool doing_trick,
                                             std::array<std::uint32_t, 2> busy_hands, bool mirrored,
                                             std::optional<PrelandingPhysical> prelanding, const Settings& settings);
};

}  // namespace s3::game::graph_host::motion_spin
