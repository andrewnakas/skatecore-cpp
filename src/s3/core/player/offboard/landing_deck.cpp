// Port of skate-core/src/player/offboard/landing_deck.rs and its query.rs,
// trajectory.rs and update.rs Manager methods.
#include "s3/core/player/offboard/landing_deck.hpp"

#include "s3/core/physics/board_motion_output.hpp"

namespace s3::player::offboard::landing_deck {

namespace m = wipeout_state::math;
using physics::board_motion_output::inverse_length_squared;

// ---------------------------------------------------------------- trajectory.rs
namespace trajectory {

float square_root(float value) { return value == 0.0f ? 0.0f : value * inverse_length_squared(value, 2); }

std::optional<float> greatest_plane_time(Trajectory t, Vector point, Vector normal) {
    const float a = m::dot(normal, m::scale(t.acceleration, m::reciprocal(2.0f)));
    const float b = m::dot(normal, t.velocity);
    const float c = m::dot(normal, t.position) - m::dot(point, normal);
    const float discriminant = b * b - (4.0f * a) * c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    } else if (discriminant > 0.0f) {
        const float root = square_root(discriminant);
        const float inverse = m::reciprocal(2.0f * a);
        return rs::max(inverse * (-b + root), inverse * (-b - root));
    } else {
        const float time = m::reciprocal(2.0f * a) * -b;
        if (time > 0.0f) return time;
        return std::nullopt;
    }
}

void adjust(Trajectory& t, std::int32_t frame, Vector delta, float maximum) {
    if (frame <= 0) {
        return;
    }
    const Vector correction = m::scale(delta, m::reciprocal(static_cast<float>(frame) * STEP));
    const float factor =
        m::dot(correction, correction) > maximum * maximum ? maximum / m::length(correction) : 1.0f;
    t.velocity = m::madd(correction, factor, t.velocity);
}

void shift(Trajectory& t, std::int32_t frame) {
    const float time = static_cast<float>(frame) * STEP;
    const Vector position = t.position_at(time);
    t.velocity = t.velocity_at(time);
    t.position = position;
}

}  // namespace trajectory

// ---------------------------------------------------------------- landing_deck.rs
void Manager::reset() {
    trajectory_32 = EMPTY;
    elapsed_160 = 0.0f;
    trajectory_valid_164 = false;
    ik_offset_176 = Vector{};
    vector_192 = Vector{};
    moving_contact_208 = Vector{};
    vector_224 = Vector{};
    obstruction_height_240 = 0.0f;
    time_to_land_244 = 0.0f;
    proposed_time_248 = 0.0f;
    completed_queries_252 = 0;
    can_land_256 = false;
    force_257 = false;
    blocked_258 = false;
    tested_259 = false;
    hippy_hurdling_260 = false;
    publish_moving_contact_261 = false;
}

void Manager::correct_trajectory(Vector com) {
    const Vector error = m::sub(trajectory_32.position_at(elapsed_160), com);
    const std::int32_t remaining = trajectory::frames(time_to_land_244);
    if (remaining > 0) {
        trajectory_32.position = m::add(trajectory_32.position, error);
        trajectory::adjust(trajectory_32, remaining, rs::map(error, [](float v) { return -v; }), 0.5f);
    }
}

FillOutput Manager::fill() const {
    FillOutput out;
    out.can_land_316 = can_land_256 && !blocked_258;
    out.hippy_hurdling_317 = hippy_hurdling_260 && !blocked_258;
    if (publish_moving_contact_261) out.moving_contact = moving_contact_208;
    return out;
}

// ---------------------------------------------------------------- query.rs
QueryRequest Manager::prepare_query(const Input& p, Vector position, float time) {
    const Vector target = m::add(m::madd(p.board_velocity_400, time, p.board_position_112),
                                 Vector{0.0f, rs::from_bits(0x3e4cccceu), 0.0f, 0.0f});
    blocked_258 = false;
    obstruction_height_240 = target[1] + -0.1f;
    QueryRequest r;
    r.trajectory.position = position;
    r.trajectory.velocity =
        m::sub(m::scale(m::sub(target, position), m::reciprocal(time)), m::scale(m::scale(GRAVITY, 0.5f), time));
    r.trajectory.acceleration = GRAVITY;
    r.trajectory.duration = time;
    r.radius = 0.3f;
    r.start_error = 0.5f;
    r.end_error = 0.5f;
    return r;
}

// ---------------------------------------------------------------- trajectory.rs (Manager)
void Manager::consider_board(const Input& p, const Settings& settings, Vector position, Vector velocity) {
    const bool airborne_board = p.wheel_contacts_2556 == 0;
    Vector board_velocity = p.board_velocity_400;
    if (airborne_board) {
        board_velocity[1] = 0.0f;
    }
    Trajectory relative;
    relative.position = position;
    relative.velocity = m::sub(velocity, board_velocity);
    relative.acceleration = airborne_board ? m::scale(GRAVITY, 0.5f) : GRAVITY;
    relative.duration = -1.0f;
    const Vector target = m::madd(p.up_544, settings.approximate_com_height, p.board_position_112);
    const std::optional<float> found = trajectory::greatest_plane_time(relative, target, p.up_544);
    if (!found || !(*found > 0.0f)) {
        can_land_256 = false;
        return;
    }
    const float time = *found;
    // 822F8BD4=9.8;82060C50=2. Min-speed time is clamped to [.75t,1.3t].
    const float distance = m::length(m::sub(position, target));
    const float optimal = trajectory::square_root(m::reciprocal(rs::from_bits(0x411ccccdu)) * (2.0f * distance));
    const float proposed_time = rs::min(time * 1.3f, rs::max(time * 0.75f, optimal));
    proposed_96.position = position;
    proposed_96.velocity = m::add(m::sub(m::scale(m::sub(target, position), m::reciprocal(proposed_time)),
                                         m::scale(m::scale(GRAVITY, 0.5f), proposed_time)),
                                  p.board_velocity_400);
    proposed_96.acceleration = GRAVITY;
    proposed_96.duration = -1.0f;
    ik_offset_176 = m::sub(target, relative.position_at(time));
    ik_offset_176[1] = 0.0f;
    time_to_land_244 = time;
    proposed_time_248 = proposed_time;
}

// ---------------------------------------------------------------- update.rs
static const Vector UP{0.0f, 1.0f, 0.0f, 0.0f};

std::optional<QueryRequest> Manager::assist(const AssistInput& input, const Settings& settings) {
    completed_queries_252 = 0;
    tested_259 = false;
    const Input& p = input.processed;
    if ((force_257 || (p.flags_2480 & 0x8000u) == 0) && p.board_up_80[1] > settings.deck_min_uprightness) {
        consider_board(p, settings, input.position, input.velocity);
        if (time_to_land_244 > 0.4f) {
            const float error = m::length(m::sub(input.velocity, proposed_96.velocity));
            trajectory_32 = proposed_96;
            trajectory_valid_164 = true;
            time_to_land_244 = proposed_time_248;
            can_land_256 = error < input.maximum_velocity_change;
        }
        if (can_land_256 && !tested_259) {
            tested_259 = true;
            return prepare_query(p, input.position, proposed_time_248);
        }
    }
    return std::nullopt;
}

UpdateOutput Manager::update(const Input& p, const Settings& settings) {
    publish_moving_contact_261 = false;
    elapsed_160 += STEP;
    if (hippy_hurdling_260) {
        const float past_apex = elapsed_160 - trajectory_32.highest_position().second;
        if (past_apex > 0.3f || (past_apex > 0.0f && time_to_land_244 < 0.2f)) {
            hippy_hurdling_260 = false;
        }
    }
    const Vector position = trajectory_32.position_at(elapsed_160);
    const Vector velocity = trajectory_32.velocity_at(elapsed_160);
    can_land_256 = !(p.mode_2540 == 6 || p.mode_2540 == 9 || p.mode_2540 == 12);
    consider_board(p, settings, position, velocity);
    // Native query decision precedes the final error/possession/upright gate.
    std::optional<QueryRequest> query;
    if (!pending_262 && !tested_259 && can_land_256) {
        query = prepare_query(p, position, time_to_land_244);
    }
    const float maximum_error = p.support_1776 < 0 ? 0.55f : 0.5f;
    can_land_256 = m::length(ik_offset_176) < maximum_error && (force_257 || (p.flags_2480 & 0x8000u) == 0) &&
                   p.board_up_80[1] > settings.deck_min_uprightness;
    const float landing_time = time_to_land_244 + elapsed_160;
    const Vector landing_velocity = trajectory_32.velocity_at(landing_time);
    const auto [apex_position, apex_time] = trajectory_32.highest_position();
    UpdateOutput out;
    out.can_land = can_land_256 && !blocked_258;
    out.trajectory_valid = trajectory_valid_164;
    out.time_to_land = time_to_land_244;
    out.elapsed = elapsed_160;
    out.landing_time = landing_time;
    out.apex_time = apex_time;
    out.position = position;
    out.landing_velocity = landing_velocity;
    out.launch_position = trajectory_32.position;
    out.landing_position = trajectory_32.position_at(landing_time);
    out.normal = UP;
    out.apex_position = apex_position;
    out.direction = m::normalize_or(landing_velocity, UP);
    out.up = UP;
    out.query = query;
    return out;
}

Vector Manager::calculate_accurate_ik_offset(const IkInput& input) {
    const Input& p = input.processed;
    const float trajectory_time = elapsed_160 + input.time_to_land;
    const float deck_time = input.time_to_land + STEP;
    const Vector local = m::sub(input.mapped_position_12608, input.animation_com_10960);
    const Frame& root = input.animation_root;
    const Vector offset = m::madd(root[2], local[2], m::madd(root[1], local[1], m::scale(root[0], local[0])));
    const Vector tangent_velocity =
        m::sub(p.board_velocity_400, m::scale(p.up_544, m::dot(p.up_544, p.board_velocity_400)));
    ik_offset_176 = m::sub(m::madd(tangent_velocity, deck_time, p.board_position_112),
                           m::add(trajectory_32.position_at(trajectory_time), offset));
    ik_offset_176[1] = 0.0f;
    return ik_offset_176;
}

}  // namespace s3::player::offboard::landing_deck
