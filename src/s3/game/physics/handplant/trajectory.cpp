// Port of skate-game/src/physics/handplant/trajectory.rs
//! Handplant Launch82D613B0, trajectory selector82D66318/66948 and COM82D625C0.
#include "s3/game/physics/handplant/trajectory.hpp"

#include "s3/core/trigonometry.hpp"
#include "s3/game/physics/air_trajectory.hpp"
#include "s3/game/physics/footplant/math.hpp"
#include "s3/game/physics/handplant.hpp"

namespace s3::game::physics::handplant {

using namespace ::s3::game::physics::footplant::math;
using namespace trajectory;
using ::s3::air::trajectory::QueryRequest;

void Handplant::launch(contact::Candidate c, V com, V velocity, V normal, V body_heading, V heading) {
    V side = normalize(cross(sub(c.edge.end, c.edge.start), UP));
    if (dot(side, normal) > 0.0f) {
        side = scale(side, -1.0f);
    }
    direction = scale(side, -1.0f);
    const V apex_p = apex_position(c.point, side, settings.apex_radius, settings.apex_angle);
    initial = to_apex(com, apex_p);
    apex = apex_time(initial);
    warped = 0.0f;
    outgoing = {initial, initial};
    travel_sign = dot(heading, velocity) > 0.0f ? 1.0f : -1.0f;
    //830BD4A0=-1 and830BD320=1; signs follow native vsel order.
    rotations[0] = rotation::frame(heading, normal);
    rotations[0][3] = com;
    V along = normalize(sub(c.edge.start, c.edge.end));
    const float dir = dot({along[0], 0.0f, along[2], 0.0f}, sub(apex_p, com)) > 0.0f ? 1.0f : -1.0f;
    along = scale(along, dir * travel_sign);
    rotations[1] = rotation::frame(along, normalize(sub(c.point, apex_p)));
    rotations[1][3] = apex_p;
    rotations[2] = rotation::frame(scale(UP, -travel_sign), normal);
    rotations[3] = rotations[2];
    estimate_apex();
    const bool changed = length(sub(previous_candidate_point, c.point)) > 0.15f;
    const bool front = dot(sub(c.point, com), body_heading) < 0.0f;
    flags = (flags & 0x0fffffffu) | 0x80000000u | (changed ? 0x40000000u : 0u) | (front ? 0x20000000u : 0u) |
            (phase < settings.committed_time ? 0x10000000u : 0u);
    anchor = c.point;
    candidate = c;
}

Status Handplant::select_outgoing(const ::s3::physics::board_world::BoardWorld& world) {
    S3G_TRY_OPT(const auto c, candidate, "Handplant entry requires the selected coping");
    const V apex_p = initial.position_at(apex);
    const V incoming = initial.velocity_at(apex);
    const V across = scale(direction, dot(incoming, direction));
    const V tangent = sub(incoming, across);
    const V along = normalize(tangent);
    const float speed = length(add(across, clamp_length(tangent, 2.0f)));
    if (c.side != 0) {
        const V expected = scale(cross(UP, direction), c.side == 1 ? 1.0f : -1.0f);
        continuation = dot(expected, along) >= 0.0f;
    }
    V position = madd(direction, 0.1f, anchor);
    position[1] = apex_p[1];
    std::optional<std::pair<V, V>> selected;
    float best_up = 1.1f;
    Trajectory final_trajectory = EMPTY;
    for (float angle : {0.0f, 0.24f, 0.48f, rs::from_bits(0x3f3851ebu), 0.96f, rs::from_bits(0x3f999999u)}) {
        const auto [sin, cos] = ::s3::trigonometry::sin_cos(angle);
        const V velocity = scale(madd(direction, sin, scale(along, cos)), speed);
        const Trajectory trajectory{position, velocity, GRAVITY, 3.0f};
        final_trajectory = trajectory;
        QueryRequest request{};
        request.trajectory = trajectory;
        request.radius = 0.2f;
        request.start_error = 1.0f;
        request.end_error = 1.0f;
        S3G_TRY(const auto hit, air_trajectory::AirTrajectoryRuntime::query(world, request));
        if (hit.contact_time < 0.0f) {
            continue;
        }
        const V contact = hit.contact_position;  //Admission uses GetPosition82D2D988.
        if (anchor[1] - contact[1] <= 0.5f) {
            continue;
        }
        const V normal = hit.landing_normal;
        //82D66B64..98/82D66BC0..BF8 evaluate the query trajectory at
        //contact time for the landing anchor, not the surface contact.
        const V point_ = trajectory.position_at(hit.contact_time);
        //The host sphere query can return a nearby ledge whose lifted
        //landing target lies above the apex. It cannot define a descending
        //arc (82D608A0 takes sqrt(2*g*drop)). Keep searching; if none is
        //feasible, use the existing native three-second fallback below.
        const V destination = madd(normal, 0.56f, point_);
        bool finite = true;
        for (float v : destination) finite &= rs::is_finite(v);
        if (!finite || !(destination[1] < apex_p[1])) {
            continue;
        }
        if (normal[1] < 0.71f) {
            selected = std::make_pair(point_, normal);
            break;
        }
        if (normal[1] < best_up) {
            best_up = normal[1];
            selected = std::make_pair(point_, normal);
        }
    }
    const auto chosen = selected ? *selected : std::make_pair(final_trajectory.position_at(3.0f), UP);
    const V landing = chosen.first;
    const V normal = chosen.second;
    const V destination = madd(normal, 0.56f, landing);
    Trajectory out = from_apex(apex_p, destination);
    const float fall_time = out.duration;
    //82D66D70 rebase the outgoing parabola to the incoming time origin.
    const V rebased_position = out.position_at(-apex);
    const V rebased_velocity = out.velocity_at(-apex);
    out.position = rebased_position;
    out.velocity = rebased_velocity;
    out.duration = -1.0f;
    outgoing = {out, out};
    const float out_speed = dot(direction, outgoing[1].velocity);
    if (out_speed < settings.minimum_out_speed) {
        outgoing[1].velocity = madd(direction, settings.minimum_out_speed - out_speed, outgoing[1].velocity);
    }
    rotations[2][3] = out.position_at(2.0f * apex);
    const float time = apex + fall_time;
    rotations[3] = rotation::frame(scale(normalize(out.velocity_at(time)), travel_sign), normal);
    rotations[3][3] = out.position_at(time);
    out_duration = rs::max(time - phase, 0.5f);
    build_curve();
    return ok();
}

void Handplant::build_curve() {
    //82D62408: secant handles one fixed tick apart, scaled by6.
    const float start = apex - settings.curve_half_time;
    const float end = apex + settings.curve_half_time;
    curve[0] = initial.position_at(start);
    curve[1] = madd(sub(initial.position_at(start + STEP), curve[0]), 6.0f, curve[0]);
    curve[3] = outgoing[1].position_at(end);
    curve[2] = madd(sub(outgoing[1].position_at(end - STEP), curve[3]), 6.0f, curve[3]);
}

std::tuple<V, V, V> Handplant::values(::s3::physics::Slice<const std::array<V, 4>> pose, V physical_com) {
    const float relative = warped - apex;
    const float fraction = (1.0f + relative * reciprocal(settings.curve_half_time)) * 0.5f;
    V com{};
    if (fraction < 0.0f) {
        com = initial.position_at(warped);
    } else if (fraction > 1.0f) {
        const float blend = clamp01((relative - settings.curve_half_time) * reciprocal(out_duration));
        com = madd(outgoing[0].position_at(warped), blend, scale(outgoing[1].position_at(warped), 1.0f - blend));
    } else {
        com = bezier(curve, fraction);
    }
    const float foot_y = rs::max(pose[15][3][1], pose[19][3][1]);
    float divisor = 3.2f;
    float offset = -0.05f;
    if ((flags & 0x20000000u) != 0) {
        divisor = 5.0f;
        offset = 0.0f;
    }
    com[1] += rs::max((foot_y - physical_com[1]) / divisor + offset, 0.0f);
    const float blend = rs::max(1.0f - elapsed * reciprocal(settings.entry_blend), 0.0f);
    const V blended = madd(com, 1.0f - blend, scale(entry.position_at(elapsed), blend));
    if (blend > 0.0f && fraction < 0.0f) {
        const V velocity = initial.velocity_at(warped);
        const float correction = dot(sub(blended, com), normalize(velocity)) * reciprocal(length(velocity));
        warped += rs::clamp(correction, rs::from_bits(0xbc088889u), rs::from_bits(0x3c088889u));
    }
    std::array<V, 4> frame{};
    if (warped < apex) {
        const float f =
            rs::max(1.0f - (apex - warped) * reciprocal(rs::min(apex, settings.rotation_time)), 0.0f);
        frame = rotation::blend(rotations[0], rotations[1], settings.into_rotation.evaluate(f));
    } else {
        const float f = warped - apex;
        const float out = settings.out_heading.evaluate(clamp01(f * reciprocal(out_duration)));
        const float into = settings.out_rotation.evaluate(clamp01(f * reciprocal(settings.rotation_time)));
        frame = rotation::blend(rotations[1], rotation::blend(rotations[2], rotations[3], out), into);
    }
    return {blended, frame[1], frame[2]};
}

namespace trajectory {

///82D61458/64 call standalone Cos/Sin. Permute mask822FBAC0 followed
///by vsldoi8 produces[sin,cos,0,0]: sine goes sideways, cosine goes up.
V apex_position(V coping, V side, float radius, float angle) {
    const float sin = ::s3::trigonometry::sin(angle);
    const float cos = ::s3::trigonometry::cos(angle);
    return madd(side, radius * sin, madd(UP, radius * cos, coping));
}

Trajectory to_apex(V start, V end) {
    const float t = rs::sqrt((-2.0f * GRAVITY[1]) * (end[1] - start[1])) * reciprocal(-GRAVITY[1]);
    return arc_between(start, end, t);
}

Trajectory from_apex(V start, V end) {
    const float t = rs::sqrt((-2.0f * GRAVITY[1]) * (start[1] - end[1])) * reciprocal(-GRAVITY[1]);
    return arc_between(start, end, t);
}

Trajectory arc_between(V start, V end, float time) {
    const V velocity = scale(sub(sub(end, start), scale(GRAVITY, 0.5f * time * time)), reciprocal(time));
    return Trajectory{start, velocity, GRAVITY, time};
}

V bezier(std::array<V, 4> p, float t) {
    const float s = 1.0f - t;
    return madd(p[3], t * t * t, madd(p[2], 3.0f * t * t * s, madd(p[0], s * s * s, scale(p[1], 3.0f * t * s * s))));
}

}  // namespace trajectory

}  // namespace s3::game::physics::handplant
