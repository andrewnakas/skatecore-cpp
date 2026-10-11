// Port of skate-core/src/physics/grind_contact/admission.rs.
// S3 82D886B8, paired with S2 IsEntryAngleAcuteEnough 82DDA748.
// Test each family/contact before arbitration, retaining its entry kind.
#pragma once
#include <array>
#include <cstdint>

#include "s3/core/point_graph.hpp"

namespace s3::physics::grind_contact {
// Declared in grind_contact.hpp; redeclared here so this header is self-contained.
using V = std::array<float, 4>;
bool within_approach_angle(V direction, V velocity, float degrees);
}  // namespace s3::physics::grind_contact

namespace s3::physics::grind_contact::admission {

enum class EntryKind : std::uint32_t {
    RideFromAbove = 0,
    RideFromBelow = 1,
    RideIntoCoping = 2,
    StayInGrind = 3,
    ChangeGrind = 4,
    AirToGrind = 5,
    DropIn = 6,
};

struct Decision {
    EntryKind kind = EntryKind::RideFromAbove;
    bool allowed = false;
};
S3_DERIVE_EQ(Decision, kind, allowed)

struct Admission {
    std::uint32_t category = 0;
    /// The current processed state2508, not the requested/previous state.
    std::uint32_t state = 0;
    float speed = 0.0f;
    V velocity{};
    /// physics_grinds/DVEntryThreshScalarVsSinSlope, X480/Y496. (Rust `&'a PointGraph<4>`.)
    const point_graph::PointGraph<4>* threshold_vs_slope = nullptr;

    Decision test(std::uint32_t requested_state, V tangent, bool allows_drop_in) const;
};

/// S3 82D861D8 / S2 CalcApproachSlopeSine 82DDA1A8.
/// This is the vertical component of normalized cross-rail velocity, not
/// rail incline or the board's forward/rail angle.
float approach_slope_sine(V tangent, V velocity);

/// S3 82D860F8 uses a smaller dead zone for coping and engagement.
float engagement_slope_sine(V tangent, V velocity);

}  // namespace s3::physics::grind_contact::admission
