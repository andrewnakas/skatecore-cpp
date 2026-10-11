// Port of skate-core/src/graph/conditions.rs.
//! ActionGraph condition leaves recovered from Skate 3 TU3.
//! Physical values are published by their real producers. An absent producer
//! is an integration error, never an implicit ground state or neutral speed.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "s3/core/animation/slice.hpp"
#include "s3/core/graph/intents.hpp"
#include "s3/core/rs.hpp"

namespace s3::graph::conditions {

using intents::IntentMap;

enum class Comparison {
    None,
    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    GreaterAbsolute,
};

struct NumericCondition {
    Comparison comparison = Comparison::None;
    float threshold = 0.0f;
    bool absolute = false;

    /// NumericCondition::Compare82C12AB8. The bge/ble branches test the
    /// absence of LT/GT, including unordered inputs; do not change to >=/<=.
    bool matches(float value) const {
        value = absolute ? rs::abs(value) : value;
        switch (comparison) {
            case Comparison::None: return false;
            case Comparison::Equal: return value == threshold;
            case Comparison::NotEqual: return value != threshold;
            case Comparison::Greater: return value > threshold;
            case Comparison::Less: return value < threshold;
            case Comparison::GreaterEqual: return !(value < threshold);
            case Comparison::LessEqual: return !(value > threshold);
            case Comparison::GreaterAbsolute: return rs::abs(value) > threshold;
        }
        return false;
    }
};
S3_DERIVE_EQ(NumericCondition, comparison, threshold, absolute)

struct SpeedInputs {
    /// PhysOut skateboard+164/+168/+192, respectively.
    float speed = 0.0f;
    float forward_speed = 0.0f;
    float speed_and_slope = 0.0f;
};
S3_DERIVE_EQ(SpeedInputs, speed, forward_speed, speed_and_slope)

struct PhysicalStateInputs {
    /// PhysOut filtered-state+0/+80/+8, respectively.
    std::uint32_t category = 0;
    bool grinding = false;
    std::string grind_name;
};
S3_DERIVE_EQ(PhysicalStateInputs, category, grinding, grind_name)

struct PushBrakeInputs {
    /// Y of ground-output+80, used by82BA5150. Its producer must be supplied;
    /// this is not interchangeable with deck up or a selected contact normal.
    float ground_axis_y = 0.0f;
    /// PhysOut skeleton+598.
    bool skeleton_disables_push_brake = false;
    /// Selected global collection316, layout+1844.
    float maximum_ground_angle_degrees = 0.0f;

    /// Complete scalar result of DisablePushBrake82BA5150. The inverse-sine
    /// estimate retains the explicitly documented numerical validation boundary.
    bool disabled() const;
};
S3_DERIVE_EQ(PushBrakeInputs, ground_axis_y, skeleton_disables_push_brake, maximum_ground_angle_degrees)

struct ConditionInputs {
    std::optional<SpeedInputs> speeds;
    std::optional<PhysicalStateInputs> physical_state;
    /// PhysOut miscellaneous+128, read by82BA15F8.
    std::optional<float> time_since_last_input;
    /// Actor stance accessors82BA4C30/82BA4C78, through component+1804.
    std::optional<bool> mirrored;
    std::optional<bool> riding_fakie;
    std::optional<PushBrakeInputs> push_brake;
};
S3_DERIVE_EQ(ConditionInputs, speeds, physical_state, time_since_last_input, mirrored, riding_fakie,
             push_brake)

/// Rust `enum ActionCondition`; each variant is a nested struct held in `value`.
struct ActionCondition {
    struct HasIntent {
        std::string name;
        NumericCondition numeric;
    };
    struct TimeSinceLastInput {
        NumericCondition _0;
    };
    struct Speed {
        bool along_skate_z = false;
        NumericCondition numeric;
    };
    struct SpeedAndSlope {
        NumericCondition _0;
    };
    struct FilteredState {
        std::uint32_t _0 = 0;
    };
    struct Grinding {
        std::optional<std::string> name;
    };
    struct Mirrored {};
    struct RidingFakie {};
    struct DisablePushBrake {};
    /// Name resolves relative to the containing state, using82C16C10. The
    /// host binds target once because the loaded hierarchy is immutable.
    struct CurrentState {
        std::string name;
        std::optional<std::size_t> target;
    };

    std::variant<HasIntent, TimeSinceLastInput, Speed, SpeedAndSlope, FilteredState, Grinding, Mirrored,
                 RidingFakie, DisablePushBrake, CurrentState>
        value;

    ActionCondition(HasIntent v) : value(std::move(v)) {}
    ActionCondition(TimeSinceLastInput v) : value(v) {}
    ActionCondition(Speed v) : value(v) {}
    ActionCondition(SpeedAndSlope v) : value(v) {}
    ActionCondition(FilteredState v) : value(v) {}
    ActionCondition(Grinding v) : value(std::move(v)) {}
    ActionCondition(Mirrored v) : value(v) {}
    ActionCondition(RidingFakie v) : value(v) {}
    ActionCondition(DisablePushBrake v) : value(v) {}
    ActionCondition(CurrentState v) : value(std::move(v)) {}

    rs::Result<bool, const char*> evaluate(const ConditionInputs& input, const IntentMap& action_intents,
                                           std::optional<std::size_t> current,
                                           animation::Slice<const std::optional<std::size_t>> parents) const;
};
S3_DERIVE_EQ(ActionCondition::HasIntent, name, numeric)
S3_DERIVE_EQ(ActionCondition::TimeSinceLastInput, _0)
S3_DERIVE_EQ(ActionCondition::Speed, along_skate_z, numeric)
S3_DERIVE_EQ(ActionCondition::SpeedAndSlope, _0)
S3_DERIVE_EQ(ActionCondition::FilteredState, _0)
S3_DERIVE_EQ(ActionCondition::Grinding, name)
S3_DERIVE_EQ_EMPTY(ActionCondition::Mirrored)
S3_DERIVE_EQ_EMPTY(ActionCondition::RidingFakie)
S3_DERIVE_EQ_EMPTY(ActionCondition::DisablePushBrake)
S3_DERIVE_EQ(ActionCondition::CurrentState, name, target)
S3_DERIVE_EQ(ActionCondition, value)

}  // namespace s3::graph::conditions
