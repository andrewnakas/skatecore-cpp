// Port of skate-core/src/player/offboard/contact_toolkit/sweep.rs (whole file).
// The toolkit's zero-acceleration, duration-one subset of82770910/8276C558.
// The query remains a swept sphere. A returned point is a surface contact,
// not the sphere centre or an unadjusted ray intersection.
#pragma once
#include <optional>
#include <vector>

#include "s3/core/player/offboard/contact_toolkit/probes.hpp"

namespace s3::player::offboard::contact_toolkit {

namespace sweep_detail {
inline float reciprocal(float value) {
    float r = physics::native_arithmetic::reciprocal_estimate(value);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r, rs::mul_add(-r, value, 1.0f), r);
    return r;
}
inline float inverse_length(float square) {
    float r = physics::reciprocal_sqrt::estimate(square);
    for (int i = 0; i < 2; ++i) r = rs::mul_add(r * 0.5f, rs::mul_add(-square, r * r, 1.0f), r);
    return r;
}
/// 82771018 chooses the most upward eligible face, then averages faces whose
/// dot with that face is strictly greater than0.5, retaining traversal order.
Vector landing_normal(const std::vector<std::array<Vector, 3>>& triangles, Vector velocity);
}  // namespace sweep_detail

/// `line(probe) -> Result<Option<LineHit>, E>`, `nearby(point, radius) -> Result<Vec<[Vector;3]>, E>`.
template <class E, class Line, class Nearby>
rs::Result<air::trajectory::QueryResult, E> query_sweep(LineProbe probe, Line&& line, Nearby&& nearby) {
    using air::trajectory::QueryResult;
    const Vector velocity = sub(probe.end, probe.start);
    // 82770910 minimum squared distance is zero for both error scalars zero.
    bool any = false;
    for (int i = 0; i < 3; ++i) any = any || rs::abs(velocity[i]) > rs::from_bits(0x37800000u);
    if (!(dot(velocity, velocity) > 0.0f) || !any) {
        return QueryResult::miss();
    }
    rs::Result<std::optional<LineHit>, E> lr = line(probe);
    if (lr.is_err()) return rs::make_err(lr.unwrap_err());
    if (!lr.unwrap().has_value()) {
        return QueryResult::miss();
    }
    const LineHit hit = *lr.unwrap();
    const float distance = length(sub(hit.position, probe.start));
    const float speed = length(velocity);
    const float epsilon = rs::from_bits(0x38d1b717u);
    const float frames =
        (rs::abs(distance) > epsilon && rs::abs(speed) > epsilon) ? sweep_detail::reciprocal(speed) * distance : 0.0f;
    // 8276C794 vrfim is floor, followed by vcfpsxws. This zero-acceleration
    // branch differs from the ceiling helper used in the accelerated branch.
    const std::int32_t contact_frame = rs::as_i32(rs::floor(frames));
    rs::Result<std::vector<std::array<Vector, 3>>, E> tr = nearby(hit.position, probe.radius);
    if (tr.is_err()) return rs::make_err(tr.unwrap_err());
    QueryResult r;
    r.contact_position = hit.position;
    r.contact_normal = hit.normal;
    r.landing_normal = sweep_detail::landing_normal(tr.unwrap(), velocity);
    r.contact_time = frames * rs::from_bits(0x3c888889u);
    r.contact_frame = contact_frame;
    r.contact_transform = hit.mesh_frame;
    r.surface = std::uint32_t(hit.surface);
    r.geometry = hit.geometry;
    return r;
}

}  // namespace s3::player::offboard::contact_toolkit
