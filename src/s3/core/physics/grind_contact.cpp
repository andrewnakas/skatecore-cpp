// Port of skate-core/src/physics/grind_contact.rs.
#include "s3/core/physics/grind_contact.hpp"

#include "s3/core/physics/grind_contact/arithmetic.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::physics::grind_contact {

std::optional<TruckContact> deck_contact(std::array<V, 4> board, std::uint32_t flags_2484, float above, float below,
                                         float deck_center_to_truck, const std::vector<Primitive>& primitives) {
    if ((flags_2484 & 0x00200000u) != 0) {
        return std::nullopt;
    }
    const V centre = add(board[3], scale(board[1], above));
    const V along = scale(board[2], deck_center_to_truck);
    const V down = scale(board[1], -(above + below));
    const V a = add(centre, along);
    const V b = sub(centre, along);
    const V c = add(a, down);
    const V d = add(b, down);
    std::optional<TruckContact> best;
    float distance = 1000000.0f;
    for (std::size_t primitive = 0; primitive < primitives.size(); ++primitive) {
        const Primitive& edge = primitives[primitive];
        const auto first = segment_triangle(edge.start, edge.end, {a, b, c});
        const auto second = segment_triangle(edge.start, edge.end, {d, b, c});
        const std::optional<V> hit = first ? first : second;
        if (hit) {
            const V position = *hit;
            const V delta = sub(position, board[3]);
            const float square = dot3(delta, delta);
            if (square < distance) {
                distance = square;
                best = TruckContact{position, primitive};
            }
        }
    }
    return best;
}

std::optional<FiftyFiftyCandidate> boardslide_candidate(std::array<V, 4> board, TruckContact contact, Primitive edge,
                                                        V velocity, std::uint32_t category, std::int32_t manager_frames,
                                                        std::uint32_t flags_2476, float deck_center_to_truck,
                                                        float truck_to_wheel, const admission::Admission& admission) {
    if ((flags_2476 & 0x40000000u) != 0) {
        return std::nullopt;
    }
    const V delta = sub(edge.end, edge.start);
    const V direction = scale(delta, arithmetic::reciprocal(arithmetic::square_root(dot3(delta, delta))));
    if (dot3(upright_normal(direction), board[1]) <= 0.65f) {
        return std::nullopt;
    }
    if (!admission.test(400, direction, false).allowed) {
        return std::nullopt;
    }
    const float depth = dot3(sub(board[3], contact.position), board[1]);
    const V projected = scale(board[1], depth);
    if (dot3(projected, projected) >= rs::from_bits(0x3b6bedfau)) {
        return std::nullopt;
    }
    if (category == 100 && rs::abs(dot3(velocity, direction)) <= 0.75f && manager_frames <= 10) {
        return std::nullopt;
    }
    V horizontal = sub(delta, scale(board[1], dot3(delta, board[1])));
    const float length = arithmetic::square_root(dot3(horizontal, horizontal));
    if (!(length > 0.0f)) {
        return std::nullopt;
    }
    horizontal = scale(horizontal, arithmetic::reciprocal(length));
    const float alignment = rs::abs(dot3(horizontal, board[2]));
    const V from = sub(contact.position, board[3]);
    const V across = sub(from, scale(board[1], dot3(from, board[1])));
    const float remaining = deck_center_to_truck - arithmetic::square_root(dot3(across, across));
    const float clearance =
        alignment > 0.0f ? remaining / alignment * arithmetic::square_root(1.0f - alignment * alignment) : 999.0f;
    if (!(clearance > truck_to_wheel)) {
        return std::nullopt;
    }
    return FiftyFiftyCandidate{direction, contact.position, edge.end, edge.start, contact.primitive};
}

std::optional<FiftyFiftyCandidate> fifty_fifty_candidate(std::array<V, 4> board, std::array<float, 2> balance,
                                                         std::array<std::optional<TruckContact>, 2> contacts) {
    return fifty_fifty_candidate_on_splines(board, balance, contacts, {});
}

std::optional<FiftyFiftyCandidate> fifty_fifty_candidate_on_splines(
    std::array<V, 4> board, std::array<float, 2> balance, std::array<std::optional<TruckContact>, 2> contacts,
    const std::vector<Primitive>& primitives) {
    if (!contacts[0] || !contacts[1]) {
        return std::nullopt;
    }
    const TruckContact front = *contacts[0];
    const TruckContact rear = *contacts[1];
    if (rs::max(rs::abs(balance[0]), rs::abs(balance[1])) >= 0.9f) {
        return std::nullopt;
    }
    if (front.primitive != rear.primitive) {
        if (front.primitive >= primitives.size()) {
            return std::nullopt;
        }
        const Primitive& edge = primitives[front.primitive];
        const V delta = sub(edge.end, edge.start);
        const V direction = scale(delta, arithmetic::inverse_square_root(dot3(delta, delta)));
        const V across = cross(upright_normal(direction), direction);
        //820641A8. This comparison is signed in the original routine.
        if (dot3(sub(front.position, rear.position), across) > 0.1f) {
            return std::nullopt;
        }
    }
    const float front_depth = dot3(sub(board[3], front.position), board[1]);
    const float rear_depth = dot3(sub(board[3], rear.position), board[1]);
    if (rs::max(front_depth, rear_depth) >= 0.13f) {
        return std::nullopt;
    }
    const V difference = sub(front.position, rear.position);
    return FiftyFiftyCandidate{
        scale(difference, arithmetic::inverse_square_root(dot3(difference, difference))),
        scale(add(front.position, rear.position), 0.5f),
        front.position,
        rear.position,
        front.primitive,
    };
}

V upright_normal(V direction) {
    const V cross_up = cross(V{0.0f, 1.0f, 0.0f, 0.0f}, direction);
    const V normal = cross(cross_up, direction);
    float length = arithmetic::square_root(dot3(normal, normal));
    if (length <= 0.0f) {
        return V{1.0f, 0.0f, 0.0f, 0.0f};
    }
    if (normal[1] < 0.0f) {
        length = -length;
    }
    return scale(normal, arithmetic::reciprocal(length));
}

bool within_approach_angle(V direction, V velocity, float degrees) {
    const V normal = upright_normal(direction);
    const V projected = sub(velocity, scale(normal, dot3(velocity, normal)));
    const float length = arithmetic::square_root(dot3(projected, projected));
    if (length <= 0.001f) {
        return true;
    }
    const float alignment = rs::abs(dot3(scale(projected, arithmetic::reciprocal(length)), direction));
    return alignment > trigonometry::cos(degrees * rs::from_bits(0x3c8efa35u));
}

std::array<std::optional<TruckContact>, 2> truck_contacts(std::array<V, 4> board, std::uint32_t flags_2484,
                                                          float truck_to_wheel, float deck_center_to_truck,
                                                          const std::vector<Primitive>& primitives) {
    if ((flags_2484 & 0x00200000u) != 0) {
        return {std::nullopt, std::nullopt};
    }
    const V right = board[0], up = board[1], forward = board[2], position = board[3];
    //821659F8 /8208EA7C: the probe starts .02 below the board and extends
    //another .20 down its local up axis. Do not substitute world vertical.
    V centre;
    for (std::size_t i = 0; i < 4; ++i) centre[i] = rs::mul_add(up[i], -0.02f, position[i]);
    const V side = scale(right, truck_to_wheel);
    const V along = scale(forward, deck_center_to_truck);
    const V down = scale(up, -0.2f);
    const std::array<V, 2> centres = {add(centre, along), sub(centre, along)};
    std::array<std::optional<TruckContact>, 2> result{};
    std::array<float, 2> distance{1000000.0f, 1000000.0f};  //822F88D4, squared-distance ceiling.
    for (std::size_t primitive = 0; primitive < primitives.size(); ++primitive) {
        const Primitive& edge = primitives[primitive];
        for (std::size_t truck = 0; truck < 2; ++truck) {
            const V a = add(centres[truck], side);
            const V b = sub(centres[truck], side);
            const V c = add(b, down);
            const V d = add(a, down);
            //Native calls both triangles and retains the first triangle's
            //intersection if both succeed, including a shared diagonal hit.
            const auto first = segment_triangle(edge.start, edge.end, {a, b, c});
            const auto second = segment_triangle(edge.start, edge.end, {a, d, c});
            const std::optional<V> hit = first ? first : second;
            if (hit) {
                const V position2 = *hit;
                const V delta = sub(position2, centres[truck]);
                const float squared = dot3(delta, delta);
                if (distance[truck] > squared) {
                    distance[truck] = squared;
                    result[truck] = TruckContact{position2, primitive};
                }
            }
        }
    }
    return result;
}

std::optional<V> segment_triangle(V start, V end, std::array<V, 3> triangle) {
    const V a = triangle[0], b = triangle[1], c = triangle[2];
    const V ac = sub(c, a);
    const V ab = sub(b, a);
    const V direction = sub(end, start);
    const V normal = cross(ab, ac);
    const float denominator = -dot3(direction, normal);
    if (denominator == 0.0f) {
        return std::nullopt;
    }
    const V from = sub(start, a);
    const float inverse = 1.0f / denominator;
    const float t = inverse * dot3(from, normal);
    if (t < 0.0f || t > 1.0f) {
        return std::nullopt;
    }
    const V side = cross(from, direction);
    const float u = inverse * dot3(ac, side);
    const float v = (1.0f / -denominator) * dot3(ab, side);
    if (u + v > 1.0f || u < 0.0f || v < 0.0f) {
        return std::nullopt;
    }
    V out;
    for (std::size_t i = 0; i < 4; ++i) out[i] = rs::mul_add(direction[i], t, start[i]);
    return out;
}

}  // namespace s3::physics::grind_contact
