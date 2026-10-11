// Port of skate-game/src/physics/footplant/update.rs.
//! Original82D6FD60 ->82D6F5F0 ->82D6F910, invoked by82D36728.
#include "s3/game/physics/footplant/update.hpp"

#include "s3/core/air/reckoning.hpp"
#include "s3/core/air/trajectory/grind.hpp"
#include "s3/core/player/offboard/air_selector/ledge.hpp"
#include "s3/core/player/offboard/ground_query.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/animated_skeleton.hpp"
#include "s3/game/physics/foot_ik.hpp"
#include "s3/game/physics/footplant.hpp"

namespace s3::game::physics::footplant {

using namespace ::s3::game::physics::footplant::math;
using sc::air::known::KnownAirFootplantInput;
using sc::air::trajectory::QueryRequest;

void Footplant::update_candidate(const KnownAirFootplantInput& input, const FootplantFrame& frame) {
    candidate = false;
    const V offset{0.0f, settings.deck_bounds_y_offset, 0.0f, 0.0f};
    const V upper = add(settings.deck_bounds, offset);
    const V lower = sub(offset, settings.deck_bounds);
    const auto& physical = frame.body.record.pose;
    const std::array<V, 2> toes{physical[15][3], physical[19][3]};
    const std::array<V, 2> board{point(frame.toolkit.inverse_effective, toes[0]),
                                 point(frame.toolkit.inverse_effective, toes[1])};
    auto outside = [&](V p) {
        for (std::size_t i = 0; i < 3; ++i) {
            if (lower[i] > p[i] || p[i] > upper[i]) return true;
        }
        return false;
    };
    physical_com = madd(input.trajectory.velocity, STEP, frame.animated.board_frames.centre_of_mass);
    animation_com = madd(input.trajectory.velocity, STEP,
                         point(frame.animated.roots.animation_to_world, frame.animated.record.centre_of_mass));
    std::size_t side = 0;
    if (outside(board[0])) {
        side = 0;
    } else if (outside(board[1])) {
        side = 1;
    } else {
        return;
    }
    const std::size_t toe = side == 0 ? 15 : 19;
    selected_toe = toe;
    candidate = true;
    selected_world = point(frame.animated.roots.animation_to_world, frame.animated.record.pose[toe][3]);
    selected_record = toes[side];
    leg_direction = normalize(sub(selected_record, physical_com));
}

void Footplant::update_launch(const KnownAirFootplantInput& input) {
    auto limit = [](V a, V b, float c) { return sc::air::reckoning::clamp_vector_within_max_angle(a, b, c); };
    if (!hit) {
        contact_time = input.remaining_collision_time;
    }
    const V future = madd(input.trajectory.acceleration, contact_time, input.trajectory.velocity);
    const V candidate_ = sub(normalize(future), input.landing_normal);
    const V from = scale(current_up, -1.0f);
    const float angle = rs::from_bits(0x420c0000u) * rs::from_bits(0x3c8efa35u);
    const V direction = normalize(limit(candidate_, from, angle));
    //82D6F7A8's second pure limiter result is discarded; v125 retains direction.
    (void)limit(sub(adjusted_contact, physical_com), from, angle);
    launch_direction = normalize(direction);
    launch_valid = true;
    request.position = madd(launch_direction, settings.leg_length_on_landing, physical_com);
    request.velocity = input.trajectory.velocity;
    //830BD330 initializer82F826B0 uses822F8B40, independently read original bytes.
    request.acceleration = {0.0f, rs::from_bits(0xc11ccccdu), 0.0f, 0.0f};
    request.duration = -1.0f;
}

Status Footplant::consume_and_submit(const KnownAirFootplantInput& input, const FootplantFrame& frame,
                                     foot_ik::FootIk& ik, sc::physics::skeleton_body::SkeletonCollisionMode& collision) {
    hit = false;
    if (enabled && result.contact_time >= 0.0f) {
        contact_time = clamp01(result.contact_time);
        contact = result.contact_position;
        surface = result.surface;
        hit = true;
        nearby_edge(frame);
        adjusted_contact = madd(input.landing_normal, settings.foot_volume_y_offset, contact);
        if (!contact_active) {
            if (candidate && contact_time > rs::from_bits(0x3de147aeu) && contact_time < 0.3f) {
                contact_active = true;
                active_elapsed = 0.0f;
            } else {
                clear_contact();
            }
        } else if (active_elapsed + contact_time < 0.0f) {
            contact_active = false;
            clear_contact();
            active_elapsed = 0.0f;
        }
        if (contact_active) {
            requested = requested || (frame.processed.flags_2480 & (1u << 23)) != 0;
            active_elapsed += STEP;
            update_pose(input, frame.animated, ik, collision);
        }
    }
    current_up = rs::from_bits(frame.processed.vectors_544_560_592_608[0]);
    Trajectory submitted;
    submitted.position = request.position;
    submitted.velocity = request.velocity;
    submitted.acceleration = frame.gravity;
    submitted.duration = 1.0f;
    QueryRequest q;
    q.trajectory = submitted;
    q.radius = rs::from_bits(0x3d23d70au);
    q.start_error = 1.0f;
    q.end_error = 1.0f;
    S3G_TRY(const auto next, air_trajectory::AirTrajectoryRuntime::query(frame.world, q));
    result = next;
    completed_trajectory = submitted;
    enabled = true;
    return ok();
}

void Footplant::clear_contact() {
    surface = 0;
    hit = false;
    contact_time = 0.0f;  //82D6FA80/82D6FAC4, distinct from Reset's -1.
    contact = {0.0f, 0.0f, 0.0f, 0.0f};
}

void Footplant::nearby_edge(const FootplantFrame& frame) {
    using sc::math::Vector3;
    using sc::player::offboard::ground_query::Edge;
    auto xyz = [](V v) { return Vector3::new_(v[0], v[1], v[2]); };
    auto lanes = [](Vector3 v) { return V{v.x, v.y, v.z, 0.0f}; };
    std::vector<Edge> edges;
    for (const auto& e : frame.edges) {
        bool inside = true;
        for (std::size_t i = 0; i < 3; ++i) {
            if (!(rs::min(e.start[i], e.end[i]) <= contact[i] + 2.0f &&
                  rs::max(e.start[i], e.end[i]) >= contact[i] - 2.0f)) {
                inside = false;
                break;
            }
        }
        if (inside) edges.push_back(Edge{xyz(e.start), xyz(e.end)});
    }
    const float original_height = contact[1];
    float best = 0.25f;  //820C6D98
    for (const auto& edge : sc::player::offboard::air_selector::ledge::filter_edges(edges, frame.toolkit.deck[3])) {
        const V start = lanes(edge.start);
        const V delta = sub(lanes(edge.end), start);
        const V raw_normal = cross(cross(delta, V{0.0f, 1.0f, 0.0f, 0.0f}), delta);
        const V normal =
            length(raw_normal) > rs::from_bits(0x358637bdu) ? normalize(raw_normal) : V{0.0f, 1.0f, 0.0f, 0.0f};
        const auto time = sc::air::trajectory::grind::candidate::descending_plane_time(completed_trajectory, start,
                                                                                         normal);
        if (!time) {
            continue;
        }
        const V arc = completed_trajectory.position_at(*time);
        const float edge_length = length(delta);
        const V direction =
            edge_length > rs::from_bits(0x37800000u) ? scale(delta, reciprocal(edge_length)) : delta;
        const V pt = madd(direction, rs::max(rs::min(dot(direction, sub(arc, start)), edge_length), 0.0f), start);
        const float distance = length(sub(arc, pt));
        //Globals3644 at822F943C permits at most 3cm below the triangle hit.
        if (distance < best && pt[1] - original_height > rs::from_bits(0xbcf5c28fu)) {
            contact_time = *time;
            current_up = normal;
            contact = pt;
            surface = 0;
            hit = true;
            best = distance;
        }
    }
}

}  // namespace s3::game::physics::footplant
