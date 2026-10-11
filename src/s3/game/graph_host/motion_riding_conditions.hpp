// Port of skate-game/src/graph_host/motion_riding_conditions.rs (module `motion_riding_conditions`).
//! Original TU3 random, COM velocity and slope conditions for stock riding.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "s3/core/graph/conditions.hpp"
#include "s3/core/rs.hpp"
#include "s3/data/state_graph.hpp"

namespace s3::game::graph_host::motion_riding_conditions {

using s3::data::state_graph_attributes::Attributes;
using s3::graph::conditions::NumericCondition;

/// Completed physical output, before the next MotionGraph evaluation.
struct RidingConditionInputs {
    /// PhysOut bundle36+16, published from Skeleton's physical COM velocity.
    std::array<float, 4> com_velocity{};
    /// PhysOutSkeleton432/464: effective animation-to-world X/Z columns.
    /// GetEffectiveRoot82BE3650 negates both iff Processed2476 bit2 is set.
    std::array<float, 4> skeleton_x{};
    std::array<float, 4> skeleton_z{};
    /// PhysOut bundle0+16.Y: the raw physical deck's up axis.
    float skate_up_y = 0.0f;
    /// PhysOutGround80.Y: retained contacting-wheel normal.
    float surface_up_y = 0.0f;
};

enum class VelocityAxis {
    X,
    Y,
    Z,
    All,
};

/// Per-MotionGraph generator. Original8258F488 seeds this once; its reset
///825953B0 leaves the sequence intact. Locking only gives the host shared
///condition interface interior mutability; it does not alter draw timing.
/// (Rust `MotionRandom(Mutex<[u32; 8]>)`: here a `mutable` array; single-threaded owner.)
class MotionRandom {
public:
    MotionRandom();
    /// `Result` kept for the Rust lock-poisoning error path (never Err here).
    rs::Result<std::uint32_t, std::string> next_u32() const;
    const std::array<std::uint32_t, 8>& words() const { return words_; }

private:
    mutable std::array<std::uint32_t, 8> words_;
};

/// Rust `enum MotionRidingCondition`; each variant is a nested struct held in `value`.
struct MotionRidingCondition {
    struct Random {};
    struct ComVelocity {
        VelocityAxis axis = VelocityAxis::All;
        NumericCondition numeric;
    };
    struct SkateSlope {
        NumericCondition _0;
    };
    struct SurfaceSlope {
        NumericCondition _0;
    };

    std::variant<Random, ComVelocity, SkateSlope, SurfaceSlope> value;

    MotionRidingCondition(Random v) : value(v) {}
    MotionRidingCondition(ComVelocity v) : value(v) {}
    MotionRidingCondition(SkateSlope v) : value(v) {}
    MotionRidingCondition(SurfaceSlope v) : value(v) {}

    static bool recognizes(std::string_view name) {
        return name == "RandomCond" || name == "ComVelCompare" || name == "SkateSlope" || name == "SurfaceSlope";
    }

    static rs::Result<MotionRidingCondition, std::string> parse(const Attributes& a);

    /// Rust `evaluate(&self, host: &MotionHost)`: reads `host.condition_random` and
    /// `host.riding_conditions` (generic here to avoid an include cycle with motion.hpp).
    template <class Host>
    rs::Result<bool, std::string> evaluate(const Host& host) const {
        return evaluate_with(host.condition_random, host.riding_conditions);
    }
    /// The body of `evaluate`, on the two MotionHost fields it reads.
    rs::Result<bool, std::string> evaluate_with(const MotionRandom& condition_random,
                                                const std::optional<RidingConditionInputs>& riding_conditions) const;
};
S3_DERIVE_EQ_EMPTY(MotionRidingCondition::Random)
S3_DERIVE_EQ(MotionRidingCondition::ComVelocity, axis, numeric)
S3_DERIVE_EQ(MotionRidingCondition::SkateSlope, _0)
S3_DERIVE_EQ(MotionRidingCondition::SurfaceSlope, _0)
S3_DERIVE_EQ(MotionRidingCondition, value)

}  // namespace s3::game::graph_host::motion_riding_conditions
