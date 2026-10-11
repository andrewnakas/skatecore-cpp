// Port of skate-core/src/physics/grind_contact/balance.rs.
// S3 TU3 GrindBalance: 82D86318, 82D86808, 82D86C88; constructor82D8A318.
// S2 counterparts82DD2168/82DD64D8/82DD26C0 corroborate field identities.
// Keep these three stages in that order, after geometry correction and before
// engagement. Independent PC arithmetic is not bit-exact Xenon emulation.
#pragma once
#include "s3/core/air/trajectory/grind_surface.hpp"
#include "s3/core/physics/grind_contact.hpp"
#include "s3/core/point_graph.hpp"

namespace s3::physics::grind_contact::balance {

using admission::EntryKind;
using air::trajectory::grind_surface::GeometryType;
using air::trajectory::grind_surface::GrindSurface;

struct BalanceContact;
struct TargetUpInput;
struct ExitLeanInput;
struct BalanceVectors;

struct BalanceState {
    float elapsed = 0.0f;
    float exit_angle_degrees = 0.0f;
    float entry_delay = 0.0f;
    std::int32_t frames_away = 0;
    /// Unrotated normal retained by82D86318, not the exit-lean output normal.
    V previous_normal{};
    V exit_direction{};

    //82D8A4BC..82D8A4EC; original vectors82139A20 and82139A10.
    static BalanceState default_() {
        return BalanceState{0.0f, 0.0f, 2.0f, 21, {0.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}};
    }

    ///82D86318. None (nullptr) is the native invalid-candidate no-write branch.
    void update_target_up(TargetUpInput input, const BalanceContact* contact,
                          BalanceVectors& vectors);

    ///82D86808. Returns investigation+396 even without a valid candidate.
    /// Graph is physics_grinds.ExitLeanAngleVsTime, eight X at+16/Y at+48.
    float update_exit_lean(ExitLeanInput input, const BalanceContact* contact,
                           const point_graph::PointGraph<8>& exit_lean_angle_vs_time, BalanceVectors& vectors);

    ///82D86C88. The callback must execute the real thin nearest world-line
    /// query82E0AAD0, retaining its filtering and endpoint/edge tolerances.
    /// No query is issued outside (15,28]. Errors are not collision misses.
    /// Updates only investigation+412 bit31, and leaves flags intact on error.
    /// `world_line(ForceExitProbe)` returns rs::Result<std::optional<ForceExitHit>, E>.
    template <class E, class F>
    rs::Result<rs::Unit, E> update_force_exit(V primitive_location, std::uint32_t& investigation_flags,
                                              F&& world_line) const;
};
S3_DERIVE_EQ(BalanceState, elapsed, exit_angle_degrees, entry_delay, frames_away, previous_normal, exit_direction)

/// A valid, geometry-corrected investigation, never a synthesized surface.
struct BalanceContact {
    const GrindSurface* surface = nullptr;  // Rust `&'a GrindSurface`
    /// Investigation+0; distinct from travel-directed investigation+32.
    V primitive_direction{};
    V directed_grind_direction{};
    std::uint32_t kind = 0;
    EntryKind entry_kind = EntryKind::RideFromAbove;
};

struct TargetUpInput {
    std::uint32_t category = 0;
    /// Processed+2504, NOT current state+2508.
    std::uint32_t previous_state = 0;
    V board_up{};
};

struct ExitLeanInput {
    std::uint32_t category = 0;
    /// Processed+2508, unlike TargetUpInput::previous_state.
    std::uint32_t current_state = 0;
    /// Processed+2536; value2 freezes the elapsed-time increment.
    std::uint32_t grind_substate = 0;
    float timestep = 0.0f;
};

/// Investigation+48/+64, deliberately without a fabricated default.
struct BalanceVectors {
    V grind_normal{};
    V target_up{};
};
S3_DERIVE_EQ(BalanceVectors, grind_normal, target_up)

struct ForceExitProbe {
    V start{};
    V end{};
};
S3_DERIVE_EQ(ForceExitProbe, start, end)

struct ForceExitHit {
    V normal{};
};

// Private helpers of balance.rs, exposed for the ported unit tests.
V negate(V value);
///82C1E220 and82D86BC0..C70: quaternion half-angle rotation, not a
/// Rodrigues replacement with a different operation order.
V rotate(V axis, V value, float angle);

template <class E, class F>
rs::Result<rs::Unit, E> BalanceState::update_force_exit(V primitive_location, std::uint32_t& investigation_flags,
                                                        F&& world_line) const {
    bool force_exit;
    if (exit_angle_degrees > 28.0f) {
        force_exit = true;
    } else if (exit_angle_degrees > 15.0f) {
        const V start = sub(primitive_location, scale(exit_direction, 0.1f));
        const V end = add(start, V{0.0f, -5.0f, 0.0f, 0.0f});
        rs::Result<std::optional<ForceExitHit>, E> r = world_line(ForceExitProbe{start, end});
        if (r.is_err()) {
            return rs::make_err(r.unwrap_err());
        }
        const std::optional<ForceExitHit>& hit = r.unwrap();
        force_exit = hit ? dot3(exit_direction, hit->normal) > -0.1f : true;
    } else {
        force_exit = false;
    }
    investigation_flags = (investigation_flags & 0x7fffffffu) | (static_cast<std::uint32_t>(force_exit) << 31);
    return rs::Unit{};
}

}  // namespace s3::physics::grind_contact::balance
