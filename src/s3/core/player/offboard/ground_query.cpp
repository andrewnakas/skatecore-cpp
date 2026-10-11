// Port of skate-core/src/player/offboard/ground_query/{consume,geometry,packet}.rs.
#include "s3/core/player/offboard/ground_query.hpp"

#include "s3/core/player/offboard/ground_query/math.hpp"

namespace s3::player::offboard::ground_query {

using namespace ground_query::math;

// ---- consume.rs ----
GroundGeometry interpret_hits(const GroundQueryPacket& packet, const std::array<std::optional<LineHit>, 7>& hits) {
    auto h = [&](std::size_t i) { return hits[i].has_value(); };
    const Vector3 up_axis = Vector3::new_(0.0f, 1.0f, 0.0f);
    Vector3 side = scale(cross(up_axis, packet.tangent),
                         inverse_root(dot(cross(up_axis, packet.tangent), cross(up_axis, packet.tangent))));
    std::uint32_t kind;
    if ((h(0) && h(1)) || h(4)) {
        kind = 3;
    } else if (h(0)) {
        kind = (hits[2] && !(hits[2]->fraction >= 0.65f)) ? 2 : 1;
    } else if (h(1)) {
        side = scale(side, -1.0f);
        kind = (hits[3] && !(hits[3]->fraction >= 0.65f)) ? 2 : 1;
    } else {
        kind = 0;
    }
    const bool flag28 = (hits[0] && (hits[0]->packed_surface & 0x0f80) == 0x0400) ||
                        (hits[1] && (hits[1]->packed_surface & 0x0f80) == 0x0400);
    bool flag26 = true, flag27 = false;
    if (hits[6]) {
        const LineHit hit = *hits[6];
        const Vector3 delta = sub(hit.position, packet.center);
        const float d = dot(hit.face_normal, safe_unit(packet.tangent, Vector3::ZERO));
        const Vector3 rejected = d > 0.0f ? sub(hit.face_normal, scale(packet.tangent, d)) : hit.face_normal;
        const Vector3 normal = safe_unit(rejected, Vector3::ZERO);
        flag26 = !(delta.y > -1.0f);
        flag27 = 0.9f > normal.y && dot(horizontal(delta), normal) > 0.0f;
    }
    return GroundGeometry{Frame{side, packet.up, packet.tangent, packet.center}, kind, flag26, flag27, flag28};
}

GroundAdjustment consume_geometry(const ConsumeInput& input, const std::optional<GroundGeometry>& geometry) {
    GroundAdjustment out{false, false, false, Frame::IDENTITY, input.previous_input_up_416};
    bool nearby = false;
    if (geometry) {
        const GroundGeometry& g = *geometry;
        const Vector3 delta = sub(g.frame.position, input.frame_80.position);
        const Vector3 dh = horizontal(delta);
        nearby = 0.3f > rs::abs(delta.y) && rs::from_bits(0x3d23d70bu) > dot(dh, dh) &&
                 rs::abs(dot(g.frame.forward, input.frame_80.forward)) > 0.9f;
        if (!g.flag28 && input.reach_364 > length(dh) + 0.7f) {
            out.state_752 = g.kind == 2 && g.frame.up.y > 0.98f;
            out.state_754 = g.flag26 || g.flag27;
        }
        if (g.kind == 0 || g.kind == 1) {
            out.state_753 = !(rs::abs(dot(delta, g.frame.right)) > 0.06f) &&
                            !((input.contact_flags_368 & 0x10u) != 0 && (input.contact_flags_368 & 0x20u) != 0);
        }
        if (out.state_752 || out.state_753) {
            out.frame_768 = g.frame;
        }
    }
    if (!out.state_753 && (input.contact_flags_368 & 1u) != 0 && (input.contact_flags_368 & 0x38u) == 0) {
        const float h = dot(sub(input.frame_80.position, input.contact_position_192), input.frame_80.up);
        // Native bge/ble leave unordered values on the accepting path.
        if (!(h >= 0.06f) && !(h <= -0.02f)) {
            out.state_753 = true;
            out.frame_768 = input.frame_80;
            out.frame_768.position = input.contact_position_192;
        }
    }
    if (nearby || out.state_753) {
        Vector3 x = horizontal(out.frame_768.right);
        const float x_length = length(x);
        if (!(x_length <= 0.001f)) {
            x = scale(x, reciprocal(x_length));
            Vector3 z = sub(out.frame_768.forward, scale(x, dot(out.frame_768.forward, x)));
            const float z_length = length(z);
            if (!(z_length <= 0.001f)) {
                z = scale(z, reciprocal(z_length));
                out.frame_768.right = x;
                out.frame_768.forward = z;
                out.frame_768.up = cross(z, x);
            }
        }
        if (0.0f > out.frame_768.up.y) {
            out.frame_768.up = scale(out.frame_768.up, -1.0f);
            out.frame_768.right = scale(out.frame_768.right, -1.0f);
        }
        out.input_up_416 = out.frame_768.up;
    }
    return out;
}

// ---- geometry.rs ----
EdgeSearch edge_search(const Frame& frame, QueryContext context, Vector3 state_vector_1040,
                       std::uint32_t processed_2488) {
    const bool narrow_forward =
        (processed_2488 & 0x08000000u) != 0 || !(dot(state_vector_1040, state_vector_1040) >= 1.0f);
    const Vector3 center = narrow_forward ? madd(frame.forward, 0.75f, frame.position) : frame.position;
    const Vector3 extent =
        madd(abs(frame.forward), 0.75f, madd(abs(frame.up), 0.2f, scale(abs(frame.right), 0.5f)));
    return EdgeSearch{sub(center, extent), add(center, extent), frame, context, narrow_forward};
}

Vector3 closest_point(Vector3 point, Edge edge) {
    Vector3 d = sub(edge.end, edge.start);
    const float l = length(d);
    if (l > rs::from_bits(0x37800000u)) {
        d = scale(d, reciprocal(l));
    }
    const float t = rs::max(rs::min(dot(d, sub(point, edge.start)), l), 0.0f);
    return madd(d, t, edge.start);
}

std::optional<EdgeSelection> select_edge(const EdgeSearch& search, const std::vector<Edge>& candidates) {
    float best_vertical = 0.2f;
    float best_horizontal = 1.5f;
    std::optional<EdgeSelection> selected;
    const std::size_t n = candidates.size() < 40 ? candidates.size() : 40;
    for (std::size_t i = 0; i < n; ++i) {
        const Edge edge = candidates[i];
        const Vector3 closest = closest_point(search.frame.position, edge);
        const Vector3 delta = sub(closest, search.frame.position);
        if (search.narrow_forward && 0.0f > dot(search.frame.forward, delta)) {
            continue;
        }
        const float vertical = dot(search.frame.up, delta);
        const float horizontal_distance = length(sub(delta, scale(search.frame.up, vertical)));
        // Native scalar bge rejects ordered >=; unordered does not take bge.
        if (!(rs::abs(vertical) >= best_vertical) && !(horizontal_distance >= best_horizontal)) {
            best_vertical = rs::abs(vertical);
            best_horizontal = horizontal_distance;
            selected = EdgeSelection{edge, closest};
        }
    }
    return selected;
}

// ---- packet.rs ----
std::optional<GroundQueryPacket> prepare_packet(const Frame& frame, QueryContext context,
                                                const EdgeSelection& selected, float deck_half_wheelbase) {
    const Edge edge = selected.edge;
    const Vector3 closest = selected.closest;
    const Vector3 reverse = safe_unit(sub(edge.start, edge.end), frame.right);
    const Vector3 forward =
        safe_unit(sub(frame.forward, scale(reverse, dot(reverse, frame.forward))), frame.forward);
    const Vector3 seventh_start = madd(forward, 0.2f, closest);
    const Vector3 delta = sub(edge.end, edge.start);
    const Vector3 raw_up = cross(delta, cross(Vector3::new_(0.0f, 1.0f, 0.0f), delta));
    const Vector3 up = safe_unit(raw_up, raw_up);
    const Vector3 tangent = safe_unit(delta, delta);
    if (rs::from_bits(0x3727c5acu) > length(raw_up)) {
        return std::nullopt;
    }
    const Vector3 projection = madd(tangent, dot(sub(closest, edge.start), tangent), edge.start);
    const Vector3 center = sub(closest, sub(closest, projection));
    const Vector3 side = cross(up, tangent);
    const Vector3 short_up = scale(up, 0.04f);
    const Vector3 near = scale(side, 0.09f);
    const Vector3 far = scale(side, deck_half_wheelbase);
    const Vector3 far_up = scale(up, deck_half_wheelbase * rs::from_bits(0x3f87ae14u));
    auto line = [](Vector3 c, Vector3 d, float radius) { return Line{add(c, d), sub(c, d), radius}; };
    GroundQueryPacket packet;
    packet.context = context;
    packet.center = center;
    packet.up = up;
    packet.tangent = tangent;
    packet.lines = {
        line(add(center, near), short_up, 0.0f),
        line(sub(center, near), short_up, 0.0f),
        line(add(center, far), far_up, 0.0f),
        line(sub(center, far), far_up, 0.0f),
        line(center, short_up, 0.0f),
        line(add(center, scale(up, 0.09f)), near, 0.001f),
        Line{seventh_start, add(seventh_start, Vector3::new_(0.0f, -6.0f, 0.0f)), 0.0f},
    };
    return packet;
}

}  // namespace s3::player::offboard::ground_query
