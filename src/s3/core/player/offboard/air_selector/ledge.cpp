// Port of skate-core/src/player/offboard/air_selector/ledge.rs (whole file).
#include "s3/core/player/offboard/air_selector/ledge.hpp"

#include "s3/core/physics/board_motion_output.hpp"
#include "s3/core/player/offboard/air_selector.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::air_selector::ledge {

using namespace math;
using s3::math::Vector3;

namespace {
Vector lanes(Vector3 v) { return Vector{v.x, v.y, v.z, 0.0f}; }
Vector3 vec3(Vector v) { return Vector3::new_(v[0], v[1], v[2]); }

Vector closest(Vector point, const Edge& edge) {
    const Vector start = lanes(edge.start);
    Vector delta = sub(lanes(edge.end), start);
    const float n = length(delta);
    if (n > rs::from_bits(0x37800000u)) {
        delta = scale(delta, reciprocal(n));
    }
    return madd(delta, rs::max(rs::min(dot(delta, sub(point, start)), n), 0.0f), start);
}

float angle(Vector new_, Vector old, Vector up) {
    if (!(dot(new_, new_) * dot(old, old) > rs::from_bits(0x37800000u))) {
        return 0.0f;
    }
    Vector a = sub(new_, scale(up, dot(up, new_)));
    Vector b = sub(old, scale(up, dot(up, old)));
    const float aa = dot(a, a), bb = dot(b, b);
    if (!(aa > 0.0001f && bb > 0.0001f)) {
        return 0.0f;
    }
    // 8296EC98 uses ONE refinement, not the two-refinement generic normalize.
    a = scale(a, physics::board_motion_output::inverse_length_squared(aa, 1));
    b = scale(b, physics::board_motion_output::inverse_length_squared(bb, 1));
    float ang = trigonometry::acos(rs::min(rs::max(dot(a, b), -1.0f), 1.0f));
    if (dot(cross(a, b), up) < 0.0f) ang = rs::f32::consts::TAU - ang;
    return rs::abs(wrap_angle(ang));
}
}  // namespace

std::optional<EdgeSearch> search(const Candidate& candidate, const Prediction& prediction, const Context& context) {
    if (!prediction.result.valid() || prediction.result.contact_frame < 16) {
        return std::nullopt;
    }
    const Vector collision = prediction.collision_position();
    const Vector apex = candidate.trajectory.highest_position().first;
    EdgeSearch s;
    s.min = vec3(rs::from_fn<4>([&](std::size_t i) { return rs::min(collision[i], apex[i]) - 2.0f; }));
    s.max = vec3(rs::from_fn<4>([&](std::size_t i) { return rs::max(collision[i], apex[i]) + 2.0f; }));
    s.frame = ground_query::Frame::IDENTITY;
    s.frame.position = vec3(candidate.trajectory.position);
    s.context.selection_flags_2948 = context.selection_flags_2948;
    s.context.matching_id_2952 = context.matching_group_2952;
    s.narrow_forward = false;
    return s;
}

std::vector<Edge> filter_edges(const std::vector<Edge>& edges, Vector reference) {
    std::vector<Vector> directions;
    directions.reserve(edges.size());
    for (const auto& e : edges) directions.push_back(normalize(sub(lanes(e.end), lanes(e.start))));
    std::vector<Edge> out;
    for (std::size_t i = 0; i < edges.size() && out.size() < 40; ++i) {
        const Edge edge = edges[i];
        const Vector start = lanes(edge.start);
        const Vector midpoint = scale(add(start, lanes(edge.end)), 0.5f);
        bool removed = false;
        for (std::size_t j = 0; j < edges.size(); ++j) {
            if (i == j) {
                continue;
            }
            const Edge& other = edges[j];
            const Vector parallel = cross(directions[i], directions[j]);
            if (!(dot(parallel, parallel) < 0.0001f)) {
                continue;
            }
            const Vector delta = sub(lanes(other.start), start);
            const Vector off = sub(delta, scale(directions[j], dot(directions[j], delta)));
            const float distance = dot(off, off);
            if (!(distance > 0.0001f && distance < 0.010000001f)) {
                continue;
            }
            const Vector from_mid = sub(lanes(other.start), midpoint);
            if (!(dot(from_mid, sub(lanes(other.end), midpoint)) < 0.0f)) {
                continue;
            }
            const Vector other_point = add(midpoint, sub(from_mid, scale(directions[j], dot(directions[j], from_mid))));
            const Vector a = sub(midpoint, reference);
            const Vector b = sub(other_point, reference);
            const Vector normal = normalize_or(cross(directions[i], cross(UP, directions[i])), Vector{});
            const float height = dot(delta, normal);
            if ((height > -0.02f && dot(a, a) > dot(b, b)) || height > 0.02f) {
                removed = true;
                break;
            }
        }
        if (!removed) out.push_back(edge);
    }
    return out;
}

std::optional<float> plane_time(Trajectory t, Vector point, Vector normal) {
    const float a = dot(normal, scale(t.acceleration, 0.5f));
    const float b = dot(normal, t.velocity);
    const float c = dot(normal, t.position) - dot(point, normal);
    const float discriminant = b * b - (4.0f * a) * c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }
    const float denominator = reciprocal(2.0f * a);
    if (discriminant > 0.0f) {
        const float root = discriminant * physics::board_motion_output::inverse_length_squared(discriminant, 2);
        return rs::max(denominator * (-b + root), denominator * (-b - root));
    }
    const float time = denominator * (-b);
    if (time > 0.0f) return time;
    return std::nullopt;
}

void Adjustment::apply(Candidate& candidate) const {
    candidate.trajectory = lowered_trajectory;
    candidate.trajectory.position = add(candidate.trajectory.position, Vector{0.0f, radius, 0.0f, 0.0f});
    const Vector d = sub(lanes(edge.end), lanes(edge.start));
    candidate.normal_64 = normalize_or(cross(d, cross(UP, d)), UP);
    candidate.contact_position_96 = point;
    candidate.landing_frame_116 = landing_frame;
    candidate.contact_velocity_80 = lowered_trajectory.velocity_at(static_cast<float>(landing_frame) * DT);
    candidate.special_121 = true;
}

std::optional<Adjustment> choose(const Candidate& candidate, const Prediction& prediction, const Context& context,
                                 float radius, const std::vector<Edge>& edges) {
    if (!prediction.result.valid() || prediction.result.contact_frame < 16) {
        return std::nullopt;
    }
    const Trajectory t = candidate.trajectory;
    Trajectory lowered = t;
    lowered.position = add(t.position, Vector{0.0f, -radius, 0.0f, 0.0f});
    std::optional<Adjustment> best;
    for (const Edge& edge : edges) {
        const Vector delta = sub(lanes(edge.end), lanes(edge.start));
        const Vector normal = normalize_or(cross(delta, cross(UP, delta)), UP);
        const std::optional<float> time = plane_time(t, lanes(edge.start), normal);
        if (!time) {
            continue;
        }
        const Vector intersection = t.position_at(*time);
        const Vector point = closest(intersection, edge);
        if (!(normal[1] > 0.71f)) {
            continue;
        }
        const Vector horizontal = flatten(sub(point, lowered.position));
        const float distance = length(horizontal);
        if (!(distance > 0.3f)) {
            continue;
        }
        const Vector direction = scale(horizontal, 1.0f / distance);
        if (!(dot(direction, normalize_or(flatten(context.forward_224), Vector{})) > 0.71f)) {
            continue;
        }
        const std::int32_t frame = rs::as_i32(*time * 59.999996f);
        Trajectory adjusted = lowered;
        // Original delta uses the UNLOWERED continuous intersection, not a
        // rounded frame sample of the lowered working trajectory (incoming).
        adjust(adjusted, frame, sub(point, intersection), 10.0f);
        const Vector velocity_delta = sub(adjusted.velocity, t.velocity);
        const float height = point[1] - prediction.result.contact_position[1];
        if (!(height > -0.05f && dot(velocity_delta, velocity_delta) < 2.25f)) {
            continue;
        }
        const bool near_height = height < 0.05f;
        const float maximum = near_height ? 10.0f : 40.0f;
        if (near_height && !(dot(direction, normalize_or(flatten(delta), Vector{})) >= 0.71f)) {
            continue;
        }
        if (!(angle(adjusted.velocity, t.velocity, context.up_544) < maximum * rs::from_bits(0x3c8efa35u))) {
            continue;
        }
        if (best && best->point[1] > point[1]) {
            continue;
        }
        best = Adjustment{edge, point, adjusted, frame, radius};
    }
    return best;
}

std::optional<std::array<Line, 6>> lines(const Adjustment& adjustment, float half_wheelbase) {
    const Edge edge = adjustment.edge;
    const Vector delta = sub(lanes(edge.end), lanes(edge.start));
    const Vector raw_up = cross(delta, cross(UP, delta));
    if (0.00001f > length(raw_up)) {
        return std::nullopt;
    }
    const Vector up = normalize_or(raw_up, raw_up);
    const Vector tangent = normalize_or(delta, delta);
    const Vector start = lanes(edge.start);
    const Vector projection = madd(tangent, dot(sub(adjustment.point, start), tangent), start);
    const Vector center = sub(adjustment.point, sub(adjustment.point, projection));
    const Vector side = cross(up, tangent);
    const Vector near = scale(side, 0.09f);
    const Vector far = scale(side, half_wheelbase);
    const Vector short_up = scale(up, 0.04f);
    const Vector far_up = scale(up, half_wheelbase * rs::from_bits(0x3f87ae14u));
    auto line = [](Vector c, Vector d, float radius) { return Line{vec3(add(c, d)), vec3(sub(c, d)), radius}; };
    return std::array<Line, 6>{
        line(add(center, near), short_up, 0.0f),
        line(sub(center, near), short_up, 0.0f),
        line(add(center, far), far_up, 0.0f),
        line(sub(center, far), far_up, 0.0f),
        line(center, short_up, 0.0f),
        line(add(center, scale(up, 0.09f)), near, 0.001f),
    };
}

rs::Result<rs::Unit, const char*> consume_lines(const std::vector<std::optional<LineHit>>& hits) {
    if (hits.size() != 6) {
        return rs::make_err("BipedAir ledge completion requires exactly six lines");
    }
    return rs::Unit{};
}

}  // namespace s3::player::offboard::air_selector::ledge
