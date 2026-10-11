// Port of skate-core/src/physics/ground_hang_geometry.rs.
#include "s3/core/physics/ground_hang_geometry.hpp"

#include <utility>

#include "s3/core/physics/board_motion_output.hpp"

namespace s3::physics::ground_hang_geometry {

using board_motion_output::dot;
using board_motion_output::inverse_length_squared;

namespace {
Vector3 cross(Vector3 a, Vector3 b) {
    return Vector3{
        rs::mul_add(-a.z, b.y, a.y * b.z),
        rs::mul_add(-a.x, b.z, a.z * b.x),
        rs::mul_add(-a.y, b.x, a.x * b.y),
    };
}
Vector3 add(Vector3 a, Vector3 b) { return Vector3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 sub(Vector3 a, Vector3 b) { return Vector3{a.x - b.x, a.y - b.y, a.z - b.z}; }
Vector3 scale(Vector3 a, float k) { return Vector3{a.x * k, a.y * k, a.z * k}; }
Vector3 madd(Vector3 a, float k, Vector3 b) {
    return Vector3{rs::mul_add(a.x, k, b.x), rs::mul_add(a.y, k, b.y), rs::mul_add(a.z, k, b.z)};
}
std::pair<Vector3, float> normalize_or_retain(Vector3 v) {
    const float square = dot(v, v);
    const float inverse = inverse_length_squared(square, 2);
    const float length = square == 0.0f ? 0.0f : square * inverse;
    return {length > rs::from_bits(0x358637bdu) ? scale(v, inverse) : v, length};
}
}  // namespace

std::optional<std::array<HangLine, 6>> hang_lines(HangGeometryInput input, float deck_center_to_truck) {
    const Vector3 delta = sub(input.edge_end, input.edge_start);
    const Vector3 first = cross(Vector3{0.0f, 1.0f, 0.0f}, delta);
    const Vector3 raw_up = cross(delta, first);
    const auto [up, length] = normalize_or_retain(raw_up);
    const Vector3 direction = normalize_or_retain(delta).first;
    if (length < rs::from_bits(0x3727c5acu)) {
        return std::nullopt;
    }
    const Vector3 on_edge = madd(direction, dot(sub(input.reference_point, input.edge_start), direction),
                                 input.edge_start);
    // Preserve the native pair of subtractions rather than canceling them.
    const Vector3 center = sub(input.reference_point, sub(input.reference_point, on_edge));
    const Vector3 side = cross(up, direction);
    const Vector3 short_up = scale(up, 0.04f);
    const Vector3 near_side = scale(side, 0.09f);
    const Vector3 far_side = scale(side, deck_center_to_truck);
    const Vector3 far_up = scale(up, deck_center_to_truck * rs::from_bits(0x3f87ae14u));
    const Vector3 last_up = scale(up, 0.09f);
    auto line = [](Vector3 c, Vector3 u, float radius) { return HangLine{add(c, u), sub(c, u), radius}; };
    return std::array<HangLine, 6>{
        line(add(center, near_side), short_up, 0.0f),
        line(sub(center, near_side), short_up, 0.0f),
        line(add(center, far_side), far_up, 0.0f),
        line(sub(center, far_side), far_up, 0.0f),
        line(center, short_up, 0.0f),
        line(add(center, last_up), near_side, 0.001f),
    };
}

rs::Result<bool, const char*> detect_hung_geometry(const BoardWorld& world, HangGeometryInput input,
                                                   float deck_center_to_truck) {
    const auto lines = hang_lines(input, deck_center_to_truck);
    if (!lines) {
        return false;
    }
    std::array<std::optional<float>, 6> fractions{};
    for (std::size_t i = 0; i < 6; ++i) {
        const HangLine& line = (*lines)[i];
        const auto hit = world.query_swept_line(line.start, line.end, line.radius);
        if (hit.is_err()) return rs::make_err(hit.unwrap_err());
        const auto& value = hit.unwrap();
        fractions[i] = value ? std::optional<float>(value->geometry.fraction) : std::nullopt;
    }
    return hung_classification(fractions);
}

bool hung_classification(std::array<std::optional<float>, 6> hits) {
    if ((hits[0].has_value() && hits[1].has_value()) || hits[4].has_value()) {
        return false;
    }
    int kind;
    if (hits[0].has_value()) {
        kind = hits[2].has_value() && *hits[2] < 0.65f ? 2 : 1;
    } else if (hits[1].has_value()) {
        kind = hits[3].has_value() && *hits[3] < 0.65f ? 2 : 1;
    } else {
        kind = 0;
    }
    return kind != 2;
}

}  // namespace s3::physics::ground_hang_geometry
