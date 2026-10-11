// Port of skate-core/src/player/offboard/biped_air/recovered.rs, height.rs,
// collision.rs, orientation.rs and post.rs (State methods).
#include "s3/core/player/offboard/biped_air/recovered.hpp"

#include "s3/core/animation/foot_ik.hpp"
#include "s3/core/player/offboard/controller/output.hpp"
#include "s3/core/player/wipeout_state/math.hpp"
#include "s3/core/player/wipeout_state/orientation.hpp"

namespace s3::player::offboard::biped_air::recovered {

namespace m = air_launch::math;

static constexpr Frame IDENTITY{{{1.0f, 0.0f, 0.0f, 0.0f},
                                 {0.0f, 1.0f, 0.0f, 0.0f},
                                 {0.0f, 0.0f, 1.0f, 0.0f},
                                 {0.0f, 0.0f, 0.0f, 0.0f}}};

// ------------------------------------------------------------------ recovered.rs
State::State() {
    frame_80 = IDENTITY;
    frame_144 = IDENTITY;
    frame_208 = IDENTITY;
    result = TrajectoryResult{};
    duration_448 = -1.0f;
    cadence = cadence::BipedCadence{};
    active = false;
    reset();
}

void State::reset() {
    flags_544_550 = {false, false, false, false, true, false, false};
    height_432 = 0.0f;
    body_offset_436 = 0.0f;
    blend_440 = 0.0f;
    time_remaining_444 = 0.0f;
    duration_448 = -1.0f;
    frame_452 = 0;
    body_target_416 = Vector{};
    result.reset();
    local_contact_464 = Vector{};
    adjustment_496 = Vector{};
    vector_512 = Vector{0.0f, 0.0f, 1.0f, 0.0f};
    restart_normal_528 = Vector{0.0f, 0.0f, 1.0f, 0.0f};
}

void State::begin_enter(const EnterInput& input) {
    reset();
    begin_enter_after_reset(input);
}

void State::begin_enter_after_reset(const EnterInput& input) {
    frame_80 = input.animation_frame;
    flags_544_550[6] = (input.flags_2484 & 0x4000u) != 0;
    frame_144 = IDENTITY;
    frame_208 = input.animation_frame;
    initial_up_480 = input.animation_frame[1];
    active = true;
}

void State::finish_enter(const EnterInput& input) {
    using height::dot;
    using height::sub;
    body_target_416 = input.body_position_15872;
    height_432 = dot(sub(input.body_position_15872, input.position_592), input.up_544);
    body_offset_436 = dot(sub(input.body_position_15936, input.body_position_15872), input.up_544);
}

rs::Result<air_launch::Packet, const char*> State::launch_packet(const air_launch::Processed& input,
                                                                 const controller::State& biped,
                                                                 const point_graph::PointGraph<8>& turn,
                                                                 air_launch::Settings settings) const {
    // 82D7BA78 overwrites104 unconditionally, so the initializer's
    // unwritten104 cannot escape the producer. No retained gameplay value.
    air_launch::Packet packet = air_launch::Packet::initialized(0.0f);
    auto r = air_launch::produce(packet, biped, turn, settings, input, false);
    if (r.is_err()) return rs::make_err(r.unwrap_err());
    return packet;
}

void State::exit(sampling::SelectorState& selector) {
    selector.pending_8492 = false;
    selector.preinitialized_8494 = false;
    active = false;
}

std::int32_t State::begin_update() {
    frame_452 = static_cast<std::int32_t>(static_cast<std::uint32_t>(frame_452) + 1u);
    return frame_452;
}

void State::update_times() {
    using height::select;
    const float duration = result.duration_388;
    duration_448 = select(duration - DT, duration, DT);
    if (result.valid_404) {
        time_remaining_444 = result.time_remaining_384;
        const float blend = 1.0f - time_remaining_444 / duration_448;
        const float lower = select(-blend, 0.0f, blend);
        blend_440 = select(1.0f - lower, lower, 1.0f);
    } else {
        blend_440 = 0.0f;
        time_remaining_444 = 10.0f;
        duration_448 = 10.0f;
    }
}

void State::update_cadence(controller::State& biped, float requested_phase) {
    using height::dot;
    using height::sub;
    if (result.valid_404) {
        biped.correction_target_592 = result.contact_position_336;
        auto& phase = biped.cadence.phase;
        if (requested_phase >= 0.0f && time_remaining_444 > 0.0f) {
            phase.forward_target = true;
            const float delta = requested_phase - phase.phase;
            phase.target = requested_phase;
            phase.duration = time_remaining_444;
            phase.rate = delta < 0.0f ? (delta + 1.0f) / time_remaining_444 : delta / time_remaining_444;
        }
        phase.advance();
        const Frame frame = biped.motion.frame_0;
        const Vector delta = sub(result.contact_position_336, frame[3]);
        local_contact_464 = {dot(frame[0], delta), dot(frame[1], delta), dot(frame[2], delta), 0.0f};
    } else {
        local_contact_464 = {0.0f, 0.0f, 1.0f, 0.0f};
    }
    cadence = biped.cadence;
}

OffBoardOutput State::output() const {
    OffBoardOutput o;
    o.scalar_32 = time_remaining_444;
    o.vector_64 = result.velocity_288;
    o.scalar_92 = duration_448;
    o.vector_96 = local_contact_464;
    o.word_144 = result.word_408;
    o.scalar_148 = result.scalar_392;
    o.scalar_152 = static_cast<float>(frame_452) * DT;
    o.scalar_156 = result.apex_time_396;
    o.vector_160 = initial_up_480;
    o.vector_176 = frame_80[3];
    o.vector_192 = result.normal_304;
    o.vector_208 = result.contact_position_336;
    o.vector_224 = frame_144[2];
    o.vector_240 = result.apex_368;
    o.flag_320 = time_remaining_444 <= DT;
    o.flag_328 = flags_544_550[4];
    o.flag_331 = true;
    return o;
}

// ------------------------------------------------------------------ height.rs
void State::correct_height(const height::HeightInput& input) {
    using namespace height;
    const float h = projected_height(input.bone15, input.bone19, result.position_272, input.up_544);
    const float limit = rs::abs(dot(input.up_544, input.velocity_608) * DT) + 0.05f;
    const float delta = (h + 0.79f) - height_432;
    const float lower = select(-limit - delta, -limit, delta);
    height_432 += select(limit - lower, lower, limit);
    body_target_416 = madd(input.up_544, height_432, result.position_272);
    const Vector predicted = madd(result.velocity_288, DT, input.bone1);
    const Vector lowered = madd(input.up_544, -0.4f, predicted);
    body_offset_436 = dot(sub(lowered, body_target_416), input.up_544);
}

// ------------------------------------------------------------------ collision.rs
namespace {
///82C1E170: remove only a positive component along the supplied direction.
///Its dot uses the normalized direction, subtraction uses the original one.
Vector remove_positive(Vector value, Vector direction) {
    const float component = m::dot(value, m::normalize_or(direction, m::ZERO));
    if (component > 0.0f) {
        return m::sub(value, m::scale(direction, component));
    }
    return value;
}
}  // namespace

collision::Response State::collision_response(const std::array<Vector, 2>& errors_16288_16304, Vector up_544,
                                              bool restart_allowed_8493) {
    // 82D2F7A8..888: the sign masks take ABSOLUTE projections and
    // absolute component products. This is not ordinary plane rejection.
    const std::array<float, 2> vertical = rs::map(errors_16288_16304, [&](Vector v) { return rs::abs(m::dot(v, up_544)); });
    std::array<float, 2> residual{};
    for (std::size_t i = 0; i < 2; ++i) {
        residual[i] = m::length(m::sub(errors_16288_16304[i],
                                       rs::map(m::scale(up_544, vertical[i]), [](float x) { return rs::abs(x); })));
    }
    // 82D2F918..938 tests BOTH projections and BOTH residual lengths.
    const bool request_52 = vertical[0] > 0.2f || vertical[1] > 0.2f || residual[0] > 0.2f || residual[1] > 0.2f;
    // 82D2F810/818/830/838: original permute inserts zeroY and repeatsX inW.
    const std::array<Vector, 2> planar =
        rs::map(errors_16288_16304, [](Vector v) { return Vector{v[0], 0.0f, v[2], v[0]}; });
    const std::array<float, 2> sizes = rs::map(planar, [](Vector v) { return m::length(v); });
    collision::Response response;
    response.request_52 = request_52;
    // 82098E1C,82098E28,82098E2C all contain3C23D70A in original S3.
    if (!(sizes[0] > 0.01f || sizes[1] > 0.01f)) {
        return response;
    }
    if (flags_544_550[6]) {
        flags_544_550[3] = true;  // 82D2FA10..20
        return response;
    }
    // 82D2FA24..40: strict greater; a tie chooses16304.
    const Vector normal = m::normalize(sizes[0] > sizes[1] ? planar[0] : planar[1]);
    const Vector velocity = result.velocity_288;
    if ((sizes[0] > 0.01f || sizes[1] > 0.01f) && m::dot(normal, velocity) < -10.0f) {
        flags_544_550[3] = true;  // 82D2FA70..FB44
    }
    if (!(m::dot(velocity, normal) < 0.0f)) {
        return response;
    }  // 82D2FB4C..5C
    const bool above_root = result.scalar_392 > frame_208[3][1] && result.normal_304[1] > 0.7f;  // 82D2FB64..BF0
    if (restart_allowed_8493 && !above_root) {
        response.restart = normal;  // 82D2FBF8..FC14
    }
    return response;
}

air_launch::Packet State::restart_packet(air_launch::Packet packet, Vector normal, Vector position_592,
                                         Vector up_544, float foot_height) {
    flags_544_550[2] = true;
    // The source computes two normalized directions; the second explicitly
    // zerosY. Both are planar here becauseF810..838 already removedY.
    const Vector horizontal = normal;
    const Vector first = remove_positive(result.velocity_288, m::scale(horizontal, -1.0f));
    const Vector second = remove_positive(first, m::scale(normal, -1.0f));
    packet.velocity_0 = m::madd(normal, 1.0f, second);                     // 82D2FCC4
    packet.board_position_80 = m::madd(up_544, foot_height, position_592);  // FD44
    packet.position_32 = m::madd(packet.velocity_0, DT, position_592);      // FD48
    packet.has_board_position_116 = true;                                  // FD30
    restart_normal_528 = normal;                                           // FD28
    return packet;
}

void State::finish_restart(const Frame& effective_animation_frame, Vector normal) {
    frame_452 = 0;  // 82D2FD5C
    flags_544_550[0] = false;
    flags_544_550[1] = false;
    frame_80 = effective_animation_frame;  // 82D2FD70..DA4
    initial_up_480 = normal;               // 82D2FDA8, NOT frame80's up
}

void State::correct_restarted_sample() {
    if (flags_544_550[2]) {
        const Vector step = m::scale(result.velocity_288, DT);
        result.position_272 =
            m::add(m::sub(result.position_272, step), remove_positive(step, m::scale(restart_normal_528, -1.0f)));
    }
}

// ------------------------------------------------------------------ orientation.rs
std::optional<Vector> State::animation_adjustment(Vector value_2880) {
    if (flags_544_550[1]) {
        if (value_2880[1] > 0.1f) {
            const Vector delta = m::sub(value_2880, adjustment_496);
            if (m::dot(delta, delta) > 0.001f) {
                adjustment_496 = m::add(adjustment_496, wipeout_state::math::limit_length(delta, 0.2f));
                return adjustment_496;
            }
        }
    } else if (flags_544_550[0] && value_2880[1] > 0.1f) {
        flags_544_550[1] = true;
        adjustment_496 = value_2880;
        return value_2880;
    }
    return std::nullopt;
}

void State::orient_sample(float start_angle_2936, std::uint32_t flags_2476) {
    if (!flags_544_550[0] && result.valid_404) {
        frame_80 = frame_208;
        Vector up = m::UP;
        if (result.normal_304[1] > 0.85f) {
            up = m::limit_angle(m::normalize_or(m::add(result.normal_304, m::UP), frame_80[1]), frame_80[1],
                                (result.time_remaining_384 * 180.0f) * m::RADIANS);
        } else if (frame_80[1][1] < 0.71f) {
            flags_544_550[5] = true;
        }
        landing_orientation(result.contact_velocity_320, up, result.time_remaining_384, start_angle_2936,
                            flags_2476);
        flags_544_550[0] = true;
        flags_544_550[2] = false;
    }
    update_times();
    frame_208 = animation::foot_ik::interpolate_native(frame_80, frame_144, blend_440).first;
    // The shared interpolation helper preserves native SIMD scratch lanes.
    // Air publishes a geometric frame, not a register image. Convert here,
    // before COM/root construction and the eventual Ground placement.
    frame_208 = rs::map(frame_208, [](Vector v) { return Vector{v[0], v[1], v[2], 0.0f}; });
}

void State::landing_orientation(Vector velocity, Vector up, float remaining, float start_angle,
                                std::uint32_t flags_2476) {
    using wipeout_state::orientation::projected_angle;
    if (flags_544_550[6]) {
        vector_512 = frame_80[2];
        const Vector horizontal = m::flatten(velocity);
        if (m::dot(horizontal, horizontal) > 0.01f) {
            const float angle = (flags_2476 & 4u) != 0 ? -start_angle : start_angle;
            vector_512 = m::normalize(m::rotate(m::normalize(horizontal), m::UP, -angle));
        }
        frame_144 = controller::build_frame(up, vector_512);
        return;
    }
    const float degrees =
        rs::abs(m::wrap_angle(projected_angle(frame_208[2], velocity, up)) * rs::from_bits(0x42652ee1u));
    if (flags_544_550[2]) {
        frame_80 = frame_208;
        if (degrees > 90.0f) {
            return;
        }
    }
    frame_144 = frame_208;
    if (remaining < 0.033f) {
        frame_80 = frame_208;
        return;
    }
    const Vector tangent = m::sub(velocity, m::scale(up, m::dot(velocity, up)));
    Vector right = m::length(tangent) > 1.0f ? m::normalize_or(m::cross(up, velocity), frame_144[0])
                                             : m::normalize_or(m::cross(up, frame_208[2]), frame_144[0]);
    if (m::length(tangent) > 1.0f && degrees > 90.0f && degrees / remaining > 540.0f) {
        flags_544_550[5] = true;
        right = m::scale(right, -1.0f);
    }
    frame_144[0] = right;
    frame_144[1] = up;
    frame_144[2] = m::normalize_or(m::cross(right, up), frame_144[2]);
}

float State::landing_assist_limit(float elapsed_2664) {
    const float time = m::select(-(elapsed_2664 - 0.034f), 0.0f, elapsed_2664 - 0.034f);
    const float blend = m::clamp(-(time * 6.25f - 1.0f), 0.0f, 1.0f);
    return (1.0f - blend) * 0.5f + blend * 3.5f;
}

void State::finish_landing_latch() {
    if (time_remaining_444 <= 0.0f && result.valid_404 && result.normal_304[1] > 0.1f) {
        flags_544_550[4] = false;
    }
}

// ------------------------------------------------------------------ post.rs
void State::post_physics(const post::PostInput& input, const post::Settings& settings, const wipeout::Frame& frame,
                         wipeout::Mode mode, Vector root_velocity, wipeout::Requests& requests) const {
    using height::dot;
    if ((input.flags_2484 & 2u) != 0) {
        post::checks::skeleton_air(requests, settings, mode, frame);
    }
    if (!result.valid_404 && input.state_timer_2664 > rs::from_bits(0x3ff33333u)) {
        requests.request(26, 0.0f);
    }
    if (flags_544_550[3]) {
        requests.request(30, 0.0f);
    }
    post::checks::offboard_air(requests, settings, mode, frame, root_velocity);
    if ((input.flags_2484 & 0x400u) != 0 && time_remaining_444 > DT) {
        requests.request(31, 0.0f);
    }
    if (flags_544_550[5] && result.velocity_288[1] < 0.0f) {
        requests.request(31, 0.0f);
    }
    if (result.valid_404 && -dot(result.normal_304, result.contact_velocity_320) > rs::from_bits(0x41473333u) &&
        time_remaining_444 < rs::from_bits(0x3e23d70au)) {
        requests.request(33, 0.0f);
    }
    if (time_remaining_444 <= 0.0f && !flags_544_550[6]) {
        const float forward = dot(input.forward_224, result.contact_velocity_320);
        const float side = dot(input.side_192, result.contact_velocity_320);
        // 82D30248 compares forward again. The decompiler aliases both.
        if (forward > 20.0f || side > 4.0f || forward < -2.0f) {
            requests.request(31, 0.0f);
        }
    }
    if (time_remaining_444 < 0.0f && flags_544_550[4]) {
        requests.request(31, 0.0f);
    }
    if (time_remaining_444 > 4.0f && (rs::abs(input.input_2708) > 0.01f || rs::abs(input.input_2704) > 0.01f)) {
        requests.request(26, 0.0f);
    }
}

}  // namespace s3::player::offboard::biped_air::recovered
