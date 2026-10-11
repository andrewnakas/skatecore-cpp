// Port of simref sim/src/camera/graph_conditions.rs (pub(super)).
//! Camera condition leaves82DF4A90..82DF61DC, bound to the stock binary graph.
#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "s3/core/camera/manager.hpp"
#include "s3/core/graph/conditions.hpp"
#include "s3/data/state_graph.hpp"
#include "s3/game/camera/graph_subject.hpp"

namespace s3::game::camera::graph_conditions {

using s3::data::state_graph_attributes::Attributes;

enum class Boolean {
    Observer,
    WipingOut,
    BrokenBone,
    Offboard,
    OffboardAir,
    OnboardAir,
    FixedInAir,
    Preparing,
    Manual,
    DroppingIn,
    MovingObject,
    Skitching,
    Footplant,
    Handplant,
    Boneless,
    HippyJump,
    HippyHurdle,
    Grinding,
    WasGrinding,
    OnGround,
    Reentry,
    Road,
    LedgeLeft,
    LedgeRight,
    WorldPainter,
    Threat,
    BadCollision,
};

enum class Value {
    Speed,
    SpeedY,
    Acceleration,
    AirTime,
    LandingHeight,
    MaxHeight,
    HeightRatio,
    FrontsideAngle,
    TransferAngle,
    LaunchIncline,
    LandingIncline,
    GrindIncline,
    Incline,
    AbsoluteIncline,
    CentredTime,
    TurningTime,
    TimeSinceInput,
};

/// Rust `enum Condition`; each variant is a nested struct held in `value`.
struct Condition {
    struct Type {
        std::uint32_t _0 = 0;
    };
    struct Previous {
        std::vector<std::uint32_t> _0;
    };
    struct Tweak {
        std::uint32_t _0 = 0;
    };
    struct Volume {
        std::string _0;
    };
    struct BooleanV {  // Rust `Boolean(Boolean)`
        Boolean _0 = Boolean::Observer;
    };
    struct Numeric {
        Value _0 = Value::Speed;
        s3::graph::conditions::NumericCondition _1;
    };
    std::variant<Type, Previous, Tweak, Volume, BooleanV, Numeric> value;

    static rs::Result<Condition, std::string> parse(const Attributes& a);

    bool evaluate(const s3::camera::CameraMan& m, const s3::camera::ManagerSubject& s, const CameraGraphSubject& g,
                  const CameraGraphEnvironment& world) const;
};

std::vector<std::string> shot_names(const Attributes& a);

}  // namespace s3::game::camera::graph_conditions
