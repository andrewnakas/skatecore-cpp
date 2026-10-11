// Port of skate-game/src/graph_host/motion_wipeout.rs (module `motion_wipeout`).
//! Stock Wipeout graph leaves, using completed physical output.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/graph/conditions.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/collections.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_wipeout {

using s3::data::state_graph_attributes::Attributes;
using s3::graph::conditions::NumericCondition;

enum class Operation {
    Wipeout,
    EnableGestures,
};

/// Live MG CA4 bits20/19 and gesture outputs CEC/CF0.
struct Controls {
    bool seed_from_air_tweak = false;
    bool gestures_enabled = false;
    std::array<float, 2> gesture{};
};

/// Wipeout instance82BC0690: counter8, angles12/16, velocity24, axes28/32, latch36.
/// (Fields are `pub(super)` in Rust.)
struct State {
    std::uint32_t ticks = 0;
    float lean = 0.0f;
    float twist = 0.0f;
    float twist_velocity = 0.0f;
    std::array<float, 2> gesture{};
    bool released = false;
};

/// (Fields are `pub(super)` in Rust.)
struct Settings {
    float threshold = 0.0f;
    float twist_velocity = 0.0f;
    float twist_acceleration = 0.0f;
    float lean_velocity = 0.0f;
    float twist_blend = 0.0f;
    float lean_blend = 0.0f;
    float gesture_y_blend = 0.0f;
    float gesture_x_blend = 0.0f;

    static rs::Result<Settings, std::string> load(const s3::data::Collections& data);
};

struct Physical {
    bool over_599 = false;
    float collision_time_144 = 0.0f;
    float no_support_time_548 = 0.0f;
    std::uint32_t profile_148 = 0;
    bool below_surface_82 = false;
    /// Skeleton output+80 lane Y; required only while below the surface.
    std::optional<float> orientation_y;
    float hips_right_angle_496 = 0.0f;
    float hips_up_angle_500 = 0.0f;
};

/// Rust `enum Condition`; each variant is a nested struct held in `value`.
struct Condition {
    struct Done {};
    struct TimeToLand {
        NumericCondition _0;
    };
    struct TimeSinceContact {
        NumericCondition _0;
    };
    struct GestureType {
        std::uint32_t _0 = 0;
    };
    struct InWater {
        std::uint32_t _0 = 0;
    };

    std::variant<Done, TimeToLand, TimeSinceContact, GestureType, InWater> value;

    Condition(Done v) : value(v) {}
    Condition(TimeToLand v) : value(v) {}
    Condition(TimeSinceContact v) : value(v) {}
    Condition(GestureType v) : value(v) {}
    Condition(InWater v) : value(v) {}

    static rs::Result<std::optional<Condition>, std::string> parse(const Attributes& a);
    rs::Result<bool, std::string> evaluate(std::optional<Physical> physical) const;
};
S3_DERIVE_EQ_EMPTY(Condition::Done)
S3_DERIVE_EQ(Condition::TimeToLand, _0)
S3_DERIVE_EQ(Condition::TimeSinceContact, _0)
S3_DERIVE_EQ(Condition::GestureType, _0)
S3_DERIVE_EQ(Condition::InWater, _0)
S3_DERIVE_EQ(Condition, value)

}  // namespace s3::game::graph_host::motion_wipeout
