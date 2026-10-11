// Port of skate-core/src/air/trajectory/grind_surface/landing.rs.
#include "s3/core/air/trajectory/grind_surface/landing.hpp"

#include "s3/core/air/trajectory/grind_surface/orientation.hpp"

namespace s3::air::trajectory::grind_surface::landing {

using trajectory::math::cross;
using trajectory::math::dot;
using trajectory::math::sub;

namespace {
/// Native XOR toggles every sign bit, including W and signed zero.
V negate(V v) {
    for (float& x : v) x = rs::from_bits(rs::to_bits(x) ^ 0x80000000u);
    return v;
}
V mul(V v, float s) { return V{v[0] * s, v[1] * s, v[2] * s, v[3] * s}; }
}  // namespace

void update_landing_orientation(const GrindSurface* surface, V takeoff_position, V grind_position, V& up,
                                LandingOrientation& out) {
    if (surface == nullptr) {
        out.kind = GeometryType::Impossible;
        out.garbage = true;
        return;
    }
    if ((surface->flags & GrindSurface::INVALID) != 0) {
        // 82C21A10..82C21A5C: completed but invalid geometry has its own
        // fully written fallback, distinct from the no-completion branch.
        const V x{1.0f, 0.0f, 0.0f, 0.0f};
        up = V{0.0f, 1.0f, 0.0f, 0.0f};
        out.kind = GeometryType::ThinRail;
        out.garbage = true;
        out.boardslide_dir = x;
        out.tipslide_dir = x;
        out.backslash_dir = negate(x);
        out.high_side = x;
        return;
    }

    up = orientation::tilted_normal(surface->direction, surface->upmost_normal, surface->normal_limits);
    out.kind = surface->kind;
    out.garbage = false;
    out.high_side = surface->high_side;
    // Original cross products are not followed by normalization.
    const V side = cross(surface->upmost_normal, surface->direction);
    const float tipslide_angle = rs::from_bits(0x3eb2b8c3u);  // 822F8618
    if (surface->kind == GeometryType::ThinRail) {
        out.boardslide_dir = side;
        const V oriented = dot(side, sub(takeoff_position, grind_position)) > 0.0f ? side : negate(side);
        const V axis = cross(oriented, surface->upmost_normal);
        out.tipslide_dir = orientation::rotate(axis, oriented, tipslide_angle);
        out.backslash_dir =
            orientation::rotate(axis, oriented, rs::from_bits(0x401a25c2u));  // 822F93B8, stored pi-minus-backslash
    } else {
        const V axis = cross(side, surface->upmost_normal);
        out.boardslide_dir = surface->kind == GeometryType::FatRail ? side : cross(up, surface->direction);
        const float sign = dot(side, surface->high_side) > 0.0f ? 1.0f : -1.0f;
        // Original sign vectors broadcast across all four lanes. These are
        // multiplies, unlike the XOR sign inversion in the thin-rail branch.
        out.backslash_dir =
            orientation::rotate(mul(axis, sign), mul(side, sign), rs::from_bits(0x3f3ba866u));  // 822F8614
        out.tipslide_dir = orientation::rotate(mul(axis, -sign), mul(side, -sign), tipslide_angle);
    }
}

}  // namespace s3::air::trajectory::grind_surface::landing
