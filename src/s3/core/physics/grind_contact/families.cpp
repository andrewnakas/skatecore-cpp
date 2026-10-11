// Port of skate-core/src/physics/grind_contact/families.rs.
#include "s3/core/physics/grind_contact/families.hpp"

#include <utility>

#include "s3/core/physics/grind_contact/arithmetic.hpp"

namespace s3::physics::grind_contact::families {

namespace {
std::optional<TruckContact> rectangle(V a, V b, V c, V d, V reference, const std::vector<Primitive>& edges) {
    std::optional<TruckContact> best;
    float distance = 1000000.0f;
    for (std::size_t primitive = 0; primitive < edges.size(); ++primitive) {
        const Primitive& edge = edges[primitive];
        std::optional<V> hit = segment_triangle(edge.start, edge.end, {a, b, c});
        if (!hit) hit = segment_triangle(edge.start, edge.end, {d, a, c});
        if (hit) {
            const V position = *hit;
            const V delta = sub(position, reference);
            const float square = dot3(delta, delta);
            if (square < distance) {
                distance = square;
                best = TruckContact{position, primitive};
            }
        }
    }
    return best;
}

FiftyFiftyCandidate geometry(TruckContact hit, Primitive edge) {
    const V delta = sub(edge.end, edge.start);
    return FiftyFiftyCandidate{
        scale(delta, arithmetic::inverse_square_root(dot3(delta, delta))),
        hit.position,
        edge.end,
        edge.start,
        hit.primitive,
    };
}
}  // namespace

std::optional<TruckContact> inverted_contact(std::array<V, 4> board, std::uint32_t flags, float epsilon, float depth,
                                             const std::vector<Primitive>& edges) {
    if ((flags & 0x00200000u) == 0) {
        return std::nullopt;
    }
    //82C1F968 uses the same diagonal as82C1FB98.
    std::array<V, 4> inverted = board;
    inverted[1] = scale(board[1], -1.0f);
    return deck_contact(inverted, 0, epsilon, depth, 0.3f, edges);
}

std::array<std::optional<TruckContact>, 2> tip_contacts(std::array<V, 4> board, std::uint32_t flags_2484,
                                                        std::uint32_t state, std::uint32_t flags_2468,
                                                        float truck_distance, const std::vector<Primitive>& edges) {
    if ((flags_2484 & 0x00200000u) != 0) {
        return {std::nullopt, std::nullopt};
    }
    std::array<std::optional<TruckContact>, 2> out;
    for (std::size_t i = 0; i < 2; ++i) {
        if ((state == 402 || state == 404) && (((flags_2468 & 0x200u) != 0) != (i == 0))) {
            out[i] = std::nullopt;
            continue;
        }
        const float sign = i == 0 ? 1.0f : -1.0f;
        const V centre = add(board[3], scale(board[1], 0.02f));
        const V a = add(centre, scale(board[2], sign * truck_distance));
        const V b = add(add(centre, scale(board[2], sign * (truck_distance + 0.215f))), scale(board[1], 0.03f));
        const V c = add(b, scale(board[1], -0.2f));
        const V d = add(a, scale(board[1], -0.2f));
        out[i] = rectangle(a, b, c, d, scale(add(a, b), 0.5f), edges);
    }
    return out;
}

bool is_backslash(V board_position, V point, std::array<V, 2> far_points, V support, float truck_distance,
                  std::uint32_t state) {
    if (state == 402) {
        return false;
    }
    const std::array<V, 2> offsets = {sub(far_points[0], point), sub(far_points[1], point)};
    const std::array<float, 2> heights = {dot3(offsets[0], support), dot3(offsets[1], support)};
    if (rs::abs(heights[0] - heights[1]) <= truck_distance * 0.1f) {
        return false;
    }
    const V higher = heights[0] > heights[1] ? offsets[0] : offsets[1];
    return dot3(sub(board_position, point), higher) > 0.0f;
}

std::optional<Contact> darkslide(std::array<V, 4> board, TruckContact hit, Primitive edge, V velocity,
                                 std::uint32_t category, std::uint32_t low_wheel_frames, const Admission& admission) {
    FiftyFiftyCandidate g = geometry(hit, edge);
    const V delta = sub(edge.end, edge.start);
    g.direction = scale(delta, arithmetic::reciprocal(arithmetic::square_root(dot3(delta, delta))));
    if (dot3(upright_normal(g.direction), scale(board[1], -1.0f)) <= 0.45f) {
        return std::nullopt;
    }
    //82D8907C deliberately tests state400, even for darkslide kind5.
    const auto decision = admission.test(400, g.direction, false);
    if (!decision.allowed) {
        return std::nullopt;
    }
    const float depth = dot3(sub(board[3], hit.position), board[1]);
    if (dot3(scale(board[1], depth), scale(board[1], depth)) >= 0.005625f) {
        return std::nullopt;
    }
    if (category == 100 && rs::abs(dot3(velocity, g.direction)) <= 0.75f && low_wheel_frames <= 10) {
        return std::nullopt;
    }
    return Contact{g, false, 5, decision.kind};
}

std::optional<Contact> five_o(std::array<V, 4> board, std::array<std::optional<TruckContact>, 2> hits,
                              const std::vector<Primitive>& edges, V velocity, std::uint32_t flags_2476,
                              std::uint32_t flags_2472, std::uint32_t ground_frames, float translation,
                              const Admission& admission) {
    if ((flags_2476 & 0x40000000u) != 0) {
        return std::nullopt;
    }
    std::array<std::optional<std::pair<FiftyFiftyCandidate, EntryKind>>, 2> valid;
    for (std::size_t k = 0; k < 2; ++k) {
        if (!hits[k]) continue;
        const TruckContact hit = *hits[k];
        const FiftyFiftyCandidate g = geometry(hit, edges.at(hit.primitive));
        const float depth = dot3(sub(board[3], hit.position), board[1]);
        const V projected = scale(board[1], depth);
        if (dot3(projected, projected) >= rs::from_bits(0x3c8a71deu)) {
            continue;
        }
        const auto decision = admission.test(403, g.direction, true);
        if (!decision.allowed || rs::abs(dot3(board[0], g.direction)) >= 0.906f ||
            rs::abs(dot3(board[1], g.direction)) >= 0.46f) {
            continue;
        }
        valid[k] = std::make_pair(g, decision.kind);
    }
    const bool forwards = dot3(board[2], velocity) > 0.0f;
    const std::size_t leading = forwards ? 0 : 1;
    const std::size_t trailing = 1 - leading;
    bool take_leading, take_trailing;
    const bool l = valid[leading].has_value(), t = valid[trailing].has_value();
    if (l && t) {
        const bool lead = ground_frames > 30 ? (flags_2472 & 0x1000u) != 0 : translation > 0.68f;
        take_leading = lead;
        take_trailing = !lead;
    } else if (!l && t) {
        take_leading = false;
        take_trailing = true;
    } else if (l && !t) {
        take_leading = ground_frames <= 30 || (flags_2472 & 0x1000u) != 0;
        take_trailing = false;
    } else {
        take_leading = false;
        take_trailing = false;
    }
    std::size_t index;
    if (take_trailing) {
        index = trailing;
    } else if (take_leading) {
        index = leading;
    } else {
        return std::nullopt;
    }
    if (!valid[index]) return std::nullopt;
    return Contact{valid[index]->first, index == 0, 3, valid[index]->second};
}

std::optional<Contact> tipslide(std::array<V, 4> board, std::array<std::optional<TruckContact>, 2> hits,
                                const std::vector<Primitive>& edges, V velocity, std::uint32_t category,
                                std::uint32_t flags_2476, std::uint32_t flags_2472, std::uint32_t ground_frames,
                                float balance, V reference_right, const Admission& admission) {
    if ((flags_2476 & 0x40000000u) != 0) {
        return std::nullopt;
    }
    const bool front = hits[0].has_value();
    const std::optional<TruckContact> selected = front ? hits[0] : hits[1];
    if (!selected) {
        return std::nullopt;
    }
    const TruckContact hit = *selected;
    const FiftyFiftyCandidate g = geometry(hit, edges.at(hit.primitive));
    const auto decision = admission.test(402, g.direction, true);
    if (!decision.allowed) {
        return std::nullopt;
    }
    if ((decision.kind == EntryKind::DropIn || (velocity[1] < 0.0f && category != 400)) &&
        board[3][1] <= hit.position[1]) {
        return std::nullopt;
    }
    const V delta = sub(board[3], hit.position);
    const float depth = dot3(delta, board[1]);
    const V projected = scale(board[1], depth);
    if (dot3(projected, projected) >= rs::from_bits(0x3b6bedfau)) {
        return std::nullopt;
    }
    const V perpendicular = sub(velocity, scale(g.direction, dot3(g.direction, velocity)));
    if (dot3(perpendicular, perpendicular) < 9.0f) {
        if (rs::abs(balance) > 0.0f) {
            if (dot3(sub(hit.position, board[3]), reference_right) >= 0.0f) {
                return std::nullopt;
            }
        } else if (ground_frames > 30) {
            const bool toward = dot3(sub(hit.position, board[3]), velocity) > 0.0f;
            if (((flags_2472 & 0x1000u) != 0) != toward) {
                return std::nullopt;
            }
        }
    }
    return Contact{g, front, 2, decision.kind};
}

}  // namespace s3::physics::grind_contact::families
