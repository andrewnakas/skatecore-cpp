// Port of skate-core/src/animation/air_leg_extension.rs.
//! ControlAirLegExtension82BAF538 and its ascending/descending/preland helpers.
#pragma once
#include <array>
#include <cstdint>
#include <optional>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/point_graph.hpp"
#include "s3/core/rs.hpp"

namespace s3::animation::air_leg_extension {

using output::attributes::AnimationAttribute;
using point_graph::PointGraph;

enum class Bone {
    Board,
    LeftToe,
    RightToe,
};

struct Settings {
    float going_up_speed = 0.0f;
    float going_down_speed = 0.0f;
    float minimum_height = 0.0f;
    float preland_velocity = 0.0f;
    float preland_final_height = 0.0f;
    float offboard_multiplier = 0.0f;
    PointGraph<4> arm_extension;
};

///All vectors are preceding completed physical output in stock coordinates.
struct Physical {
    std::array<float, 4> com_velocity{};
    std::array<float, 4> com_position{};
    std::array<float, 4> system_up{};
    std::array<float, 4> right_toe{};
    std::array<float, 4> left_toe{};
    float animation_height = 0.0f;
    bool offboard_316 = false;
    float remaining_air_time = 0.0f;
};

///CreateInstance82BAF980 zeros mode8, distance12 and descending_time16.
struct State {
    std::uint32_t mode = 0;
    float height = 0.0f;
    float descending_time = 0.0f;

    bool needs_initial_attribute() const { return mode == 0; }
    bool needs_prelanding_query(Physical p) const {
        return mode == 2 || (mode == 1 && p.com_velocity[1] < 0.0f);
    }
    ///The first update seeds the height but does not also advance its new mode.
    std::array<float, 2> update(Bone bone, Physical p, const Settings& settings, float dt,
                                std::optional<AnimationAttribute> initial_attribute, bool prepare_to_land);
    void preland(Bone bone, const Settings& settings, float dt);
};

///82BAFDB8 checks the named FullExtension attribute after the physical query.
bool prepare_to_land(bool physical_query, std::optional<AnimationAttribute> full_extension, Physical p,
                     const Settings& s);

}  // namespace s3::animation::air_leg_extension
