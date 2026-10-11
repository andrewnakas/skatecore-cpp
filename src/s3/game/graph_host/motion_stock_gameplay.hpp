// Port of skate-game/src/graph_host/motion_stock_gameplay.rs.
//! Typed bindings for the remaining stock MotionGraph gameplay operations.
//!
//! These operations are deliberately represented as authored gameplay nodes,
//! not compatibility no-ops. Their producers are completed incrementally and
//! execution fails with the exact authored operation until that producer is
//! published by the corresponding physics owner.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "s3/core/animation/output/attributes.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_stock_gameplay {

using s3::animation::output::attributes::AttributeName;
using s3::data::state_graph_attributes::Attributes;

enum class GrabType {
    Fs,
    Bs,
    Nose,
    Tail,
};

/// `GrabType::parse`.
rs::Result<GrabType, std::string> grab_type_parse(std::string_view value);
/// `GrabType::as_value`. TU3 82BB9BD0: zero is no grab; authored types are one through four.
float grab_type_as_value(GrabType self);

struct MovingObjectRegistry {
    // Private in Rust.
    std::map<std::uint32_t, std::string> objects;
    std::optional<std::uint32_t> active_;

    /// InitMovingObjects::Begin normalizes the authored resource path before
    /// hashing it with the stock DJB2 accumulator (5381, multiply by 33).
    std::uint32_t register_(std::string path);
    rs::Result<rs::Unit, std::string> begin(std::string_view path);
    rs::Result<bool, std::string> update(std::string_view path);
    void end() { active_ = std::nullopt; }
    bool active() const { return active_.has_value(); }
};

/// TU3 82BBF180..F2EC, constants at 820DDEA8..DEE4. Sector windows
/// expand when neighboring variants are absent; first matching window wins.
/// `directions` is [up, left, down, right]. (`pub(super)` in Rust.)
std::string_view select_grab_score(std::string_view base, std::array<std::optional<std::string_view>, 4> directions,
                                   float x, float y);

/// Rust `enum Operation`; each variant is a nested struct held in `value`.
struct Operation {
    struct MatchAirTime {};
    struct UpdateStandingOnCar {};
    struct InitMovingObjects {
        std::string path;
    };
    struct MovingObject {
        std::string path;
    };
    struct SetGrabType {
        std::string grab;
    };
    struct JumpInto {
        AttributeName attribute;
    };
    struct ScoringHandPlants {};
    struct SetHandPlantAnticLength {};
    struct ScoringGrabs {
        std::string grab_name;
        std::optional<std::string> up;
        std::optional<std::string> left;
        std::optional<std::string> down;
        std::optional<std::string> right;
        std::string intent_x;
        std::string intent_y;
        bool invert_y = false;
        bool polar = false;
    };
    struct AirDismounting {};
    struct TweakProject {
        std::string intent_x;
        std::string intent_y;
        AttributeName attribute_x;
        AttributeName attribute_y;
    };
    struct FootPlantAbsorb {};
    struct FingerFlipOut {
        std::string grab_intent;
    };
    struct EnterSkitchingBehaviour {};
    struct SkitchingBehaviour {};
    struct SkitchShimmyingBehaviour {};
    struct SetManualAngle {
        AttributeName attribute;
    };
    struct HippyJumpAntic {};
    struct LandOnBoard {};

    using Value = std::variant<MatchAirTime, UpdateStandingOnCar, InitMovingObjects, MovingObject, SetGrabType,
                               JumpInto, ScoringHandPlants, SetHandPlantAnticLength, ScoringGrabs, AirDismounting,
                               TweakProject, FootPlantAbsorb, FingerFlipOut, EnterSkitchingBehaviour,
                               SkitchingBehaviour, SkitchShimmyingBehaviour, SetManualAngle, HippyJumpAntic,
                               LandOnBoard>;
    Value value;

    template <class V, class = std::enable_if_t<std::is_constructible_v<Value, V>>>
    Operation(V v) : value(std::move(v)) {}

    template <class V>
    bool is() const {
        return std::holds_alternative<V>(value);
    }

    static bool recognizes(std::string_view name);
    /// Panics (Rust `unreachable!`) on a name `recognizes` rejects.
    static Operation parse(const Attributes& a);
};
S3_DERIVE_EQ_EMPTY(Operation::MatchAirTime)
S3_DERIVE_EQ_EMPTY(Operation::UpdateStandingOnCar)
S3_DERIVE_EQ(Operation::InitMovingObjects, path)
S3_DERIVE_EQ(Operation::MovingObject, path)
S3_DERIVE_EQ(Operation::SetGrabType, grab)
S3_DERIVE_EQ(Operation::JumpInto, attribute)
S3_DERIVE_EQ_EMPTY(Operation::ScoringHandPlants)
S3_DERIVE_EQ_EMPTY(Operation::SetHandPlantAnticLength)
S3_DERIVE_EQ(Operation::ScoringGrabs, grab_name, up, left, down, right, intent_x, intent_y, invert_y, polar)
S3_DERIVE_EQ_EMPTY(Operation::AirDismounting)
S3_DERIVE_EQ(Operation::TweakProject, intent_x, intent_y, attribute_x, attribute_y)
S3_DERIVE_EQ_EMPTY(Operation::FootPlantAbsorb)
S3_DERIVE_EQ(Operation::FingerFlipOut, grab_intent)
S3_DERIVE_EQ_EMPTY(Operation::EnterSkitchingBehaviour)
S3_DERIVE_EQ_EMPTY(Operation::SkitchingBehaviour)
S3_DERIVE_EQ_EMPTY(Operation::SkitchShimmyingBehaviour)
S3_DERIVE_EQ(Operation::SetManualAngle, attribute)
S3_DERIVE_EQ_EMPTY(Operation::HippyJumpAntic)
S3_DERIVE_EQ_EMPTY(Operation::LandOnBoard)
S3_DERIVE_EQ(Operation, value)

}  // namespace s3::game::graph_host::motion_stock_gameplay
