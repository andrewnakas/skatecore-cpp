// Port of skate-core/src/air/trajectory/grind_surface/classify.rs.
#include "s3/core/air/trajectory/grind_surface/classify.hpp"

#include <utility>

#include "s3/core/air/trajectory/grind_surface/orientation.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::air::trajectory::grind_surface::classify {

using namespace trajectory::math;
using Hit = std::optional<ProbeHit>;

namespace {
bool is_curb(Hit a, Hit b, V up, V center) {
    if (!a || !b) {
        return false;
    }
    if (!(dot(up, a->normal) > 0.99f && dot(up, b->normal) > 0.99f)) {
        return false;
    }
    const float ah = dot(sub(a->position, center), up);
    const float bh = dot(sub(b->position, center), up);
    const float high = ah >= bh ? ah : bh;
    const float low = ah >= bh ? bh : ah;
    return high > -0.035f && high < 0.035f && low > -0.24f && low < -0.2f;
}

bool optional_normal_test(ProbeHit hit, V center, V direction) {
    // 82C1E170 subtracts ORIGINAL direction times a positive normalized dot.
    const float component = dot(hit.normal, normalize(direction));
    const V projected = component > 0.0f ? sub(hit.normal, scale(direction, component)) : hit.normal;
    const V normal = normalize(projected);
    if (!(rs::from_bits(0x3f666666u) > normal[1])) {
        return false;
    }
    V offset = sub(hit.position, center);
    offset[1] = 0.0f;
    return dot(offset, normal) > 0.0f;
}

std::pair<std::uint32_t, std::uint32_t> unpack(std::uint32_t surface) {
    return {surface & 0x7fu, (surface >> 7) & 0x1fu};
}

std::pair<std::uint32_t, std::uint32_t> surface_info(GeometryType kind, bool curb, const std::array<Hit, 5>& hits) {
    if (curb) {
        return {4, 2};
    }
    if (kind == GeometryType::ThinRail) {
        return {hits[4] ? (hits[4]->packed_surface & 0x7fu) : 11u, 4};
    }
    // Pair priority 0/1, then 2/3, then 4. Equal heights select the second.
    for (std::size_t pair : {0u, 2u}) {
        Hit selected;
        const Hit& a = hits[pair];
        const Hit& b = hits[pair + 1];
        if (a && b) {
            selected = a->position[1] > b->position[1] ? a : b;
        } else {
            selected = a ? a : b;
        }
        if (selected) {
            return unpack(selected->packed_surface);
        }
    }
    return hits[4] ? unpack(hits[4]->packed_surface) : std::pair<std::uint32_t, std::uint32_t>{3, 1};
}
}  // namespace

GrindSurface resolve(const Investigation& plan, const std::array<Hit, 7>& hits, std::uint32_t previous_flags) {
    const Hit h0 = hits[0], h1 = hits[1], h2 = hits[2], h3 = hits[3], h4 = hits[4], h5 = hits[5];
    const Hit h6 = plan.probes.size() == 7 ? hits[6] : std::nullopt;
    const bool blocked = (h0.has_value() && h1.has_value()) || h5.has_value();
    GeometryType kind;
    if (blocked) {
        kind = GeometryType::Impossible;
    } else if (h0) {
        kind = (h2 && h2->fraction < rs::from_bits(0x3f266666u)) ? GeometryType::Ledge : GeometryType::FatRail;
    } else if (h1) {
        kind = (h3 && h3->fraction < rs::from_bits(0x3f266666u)) ? GeometryType::Ledge : GeometryType::FatRail;
    } else {
        kind = GeometryType::ThinRail;
    }
    // Native result high side uses unguarded normalization, not NormalizeSafe.
    V side = cross(UP, plan.direction);
    side = scale(side, inverse_length(dot(side, side)));
    const V high_side = (!blocked && !h0 && h1) ? scale(side, -1.0f) : side;
    const bool curb = kind == GeometryType::Ledge && is_curb(h2, h3, plan.upmost_normal, plan.center);
    bool stair = false;
    for (const Hit& h : {h0, h1}) {
        if (h && (h->packed_surface & 0xf80u) == 0x400u) stair = true;
    }
    const bool optional_normal = h6 && optional_normal_test(*h6, plan.center, plan.direction);
    const bool optional_drop = !h6 || !(h6->position[1] - plan.center[1] > -1.0f);
    std::uint32_t flags = previous_flags & ~(GrindSurface::INVALID | GrindSurface::CURB | GrindSurface::STAIR |
                                             GrindSurface::OPTIONAL_NORMAL_TEST | GrindSurface::OPTIONAL_DROP_TEST);
    if (blocked) {
        flags |= GrindSurface::BLOCKED_CROSS_SECTION;
    }
    if (curb) {
        flags |= GrindSurface::CURB;
    }
    if (stair) {
        flags |= GrindSurface::STAIR;
    }
    if (optional_normal) {
        flags |= GrindSurface::OPTIONAL_NORMAL_TEST;
    }
    if (optional_drop) {
        flags |= GrindSurface::OPTIONAL_DROP_TEST;
    }
    const auto [audio_surface, physics_surface] = surface_info(kind, curb, {h0, h1, h2, h3, h4});
    const std::array<V, 2> far_points = {
        h2 ? h2->position : plan.probes[2].end,
        h3 ? h3->position : plan.probes[3].end,
    };
    const V negative_up = scale(plan.upmost_normal, -1.0f);
    // Native first computes lengths, then separately refines their reciprocals.
    // No safe-normalize epsilon is introduced for the far-point directions.
    std::array<float, 2> angles;
    for (std::size_t i = 0; i < 2; ++i) {
        const V delta = sub(far_points[i], plan.center);
        const V unit = scale(delta, reciprocal(length(delta)));
        angles[i] = trigonometry::acos(rs::min(rs::max(dot(negative_up, unit), -1.0f), 1.0f));
    }
    const float half_pi = rs::from_bits(0x3fc90fdbu);
    const float clearance = rs::from_bits(0x3e8efa35u);
    const std::array<float, 2> normal_limits = {
        (angles[0] + half_pi) + clearance,
        ((rs::from_bits(0x40c90fdbu) - angles[1]) - half_pi) - clearance,
    };
    const V tilted_upmost_normal = orientation::tilted_normal(plan.direction, plan.upmost_normal, normal_limits);
    GrindSurface out;
    out.center = plan.center;
    out.far_points = far_points;
    out.upmost_normal = plan.upmost_normal;
    out.direction = plan.direction;
    out.high_side = high_side;
    out.normal_limits = normal_limits;
    out.kind = kind;
    out.audio_surface = audio_surface;
    out.physics_surface = physics_surface;
    out.flags = flags;
    out.tilted_upmost_normal = tilted_upmost_normal;
    return out;
}

}  // namespace s3::air::trajectory::grind_surface::classify
