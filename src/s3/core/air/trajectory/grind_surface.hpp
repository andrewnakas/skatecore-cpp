// Port of skate-core/src/air/trajectory/grind_surface.rs (+ grind_surface/*).
// Static geometry investigation from original S3 TU3 82C20728/82C20C08.
// Query execution is supplied by the world owner; a miss is not missing data.
// No moving-object observations or authored material codes are synthesized.
//
// NOTE (stage C): only this submodule of air::trajectory (plus its math.rs) has been
// ported so far, because physics::grind_contact / grind_air depend on it.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "s3/core/air/trajectory/math.hpp"

namespace s3::air::trajectory::grind_surface {

using V = std::array<float, 4>;

struct InvestigationInput {
    V start{};
    V end{};
    V reference{};
    /// Input +48, enabled by input byte +64: a six-unit world-down probe.
    std::optional<V> optional_probe;
    /// physics_grinds layout +636, supplied from the stock collection.
    float deck_center_to_truck = 0.0f;
};

struct Probe {
    V start{};
    V end{};
    float radius = 0.0f;
};
S3_DERIVE_EQ(Probe, start, end, radius)

struct ProbeHit {
    float fraction = 0.0f;
    V position{};
    V normal{};
    /// Original query result +100, NOT a rendering tag or triangle index.
    std::uint32_t packed_surface = 0;
};

struct Investigation {
    V center{};
    V upmost_normal{};
    V direction{};
    std::vector<Probe> probes;
};

enum class GeometryType : std::uint32_t {
    ThinRail = 0,
    FatRail = 1,
    Ledge = 2,
    Impossible = 3,
};

struct GrindSurface {
    V center{};
    std::array<V, 2> far_points{};
    V upmost_normal{};
    V direction{};
    V high_side{};
    std::array<float, 2> normal_limits{};
    GeometryType kind = GeometryType::ThinRail;
    std::uint32_t audio_surface = 0;
    std::uint32_t physics_surface = 0;
    /// Native output +116. Preserve unnamed bits, rather than invent meaning.
    std::uint32_t flags = 0;
    /// 82C21828 evaluated from the geometric limits; distinct from +48 upmost.
    V tilted_upmost_normal{};

    static constexpr std::uint32_t INVALID = 0x80000000u;
    static constexpr std::uint32_t CURB = 0x40000000u;
    static constexpr std::uint32_t BLOCKED_CROSS_SECTION = 0x20000000u;
    static constexpr std::uint32_t STAIR = 0x10000000u;
    static constexpr std::uint32_t OPTIONAL_NORMAL_TEST = 0x08000000u;
    static constexpr std::uint32_t OPTIONAL_DROP_TEST = 0x04000000u;

    /// 82C20C08's no-submission output; not an artificial startup blocker.
    static GrindSurface not_submitted(InvestigationInput input);
};

/// 82C20728: six fixed descriptors and the optional seventh descriptor.
/// Returns None exactly for the native inadequate-up-vector gate.
std::optional<Investigation> prepare(InvestigationInput input);

/// Explicit result processing also permits a native-style reused output flag
/// word. Bit29 is set on obstruction, not cleared on the other native branch.
GrindSurface resolve(const Investigation& plan, const std::array<std::optional<ProbeHit>, 7>& hits,
                     std::uint32_t previous_flags);

/// Full synchronous host equivalent of prepare/submit/wait/read/classify.
/// The callback must provide the original nearest-hit contract and packed
/// surface code for each probe, honoring its world-query filtering context.
/// `query(index, probe)` returns rs::Result<std::optional<ProbeHit>, E>.
template <class E, class F>
rs::Result<GrindSurface, E> investigate(InvestigationInput input, F&& query) {
    const auto plan = prepare(input);
    if (!plan) {
        return GrindSurface::not_submitted(input);
    }
    std::array<std::optional<ProbeHit>, 7> hits{};
    for (std::size_t index = 0; index < plan->probes.size(); ++index) {
        rs::Result<std::optional<ProbeHit>, E> r = query(index, plan->probes[index]);
        if (r.is_err()) {
            return rs::make_err(r.unwrap_err());
        }
        hits[index] = r.unwrap();
    }
    return resolve(*plan, hits, 0);
}

}  // namespace s3::air::trajectory::grind_surface

// `pub use landing::{LandingOrientation, update_landing_orientation};` lives at the end of
// landing.hpp so it is emitted after the child's declarations regardless of include order.
#include "s3/core/air/trajectory/grind_surface/landing.hpp"
