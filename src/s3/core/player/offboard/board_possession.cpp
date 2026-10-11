// Port of skate-core/src/player/offboard/board_possession.rs, lifecycle.rs and motion.rs.
#include "s3/core/player/offboard/board_possession.hpp"

#include "s3/core/animation/foot_ik.hpp"
#include "s3/core/trigonometry.hpp"

namespace s3::player::offboard::board_possession {

using namespace math;

Fill fill(const SkateboardControllerFields& fields, const State& state, const Processed& p, Frame bone11) {
    const Frame& frame = p.player_frame_192;
    const Vector on_axis = madd(frame[1], dot(sub(bone11[3], frame[3]), frame[1]), frame[3]);
    const Vector position = fields.state_448 == 3 ? state.retrieval.initial_0[3] : p.board_frame_64[3];
    const Vector delta = sub(position, on_axis);
    const float angle = quadrant_angle(dot(delta, frame[0]), dot(delta, frame[2]));
    const std::uint32_t s = fields.state_448;
    const std::uint32_t bits = p.flags_2476;
    Fill out;
    out.angle_36 = angle;
    out.angle_40 = 0.0f;
    out.held_311 = s == 1;
    out.free_312 = s == 2 || s == 3 || s == 4;
    out.returning_313 = s == 4;
    out.hiding_321 = s == 3;
    out.flag_322 = (bits & 0x200u) != 0;
    out.flag_323 = (bits & 0x400u) != 0 || s == 4;
    out.flag_324 = (bits & 0x400u) != 0 || ((bits & 0x800u) != 0 && s == 4);
    return out;
}

// ------------------------------------------------------------------ lifecycle.rs
void State::stop(SkateboardControllerFields& f, const Observation& o, const Settings& s, Effects& e) {
    if (f.system_on_452) {
        f.word_444 = 0;
        if (f.state_448 != 0) {
            let_go(f, o, s, e);
            f.state_448 = 0;
        }
        f.system_on_452 = false;
    }
}

void State::hold(SkateboardControllerFields& fields, const Observation& o, Effects& e) {
    e.enable_animation_soft();
    e.standard_board();
    e.collision_volumes(true);
    fields.state_448 = 1;
    selected_hand_424 = (~o.processed.flags_2476 >> 2) & 1u;
    hands[selected_hand_424].dynamics = {{{0x4395ffffu, 0, 0x468c9fffu, 2}, {0x4395ffffu, 0, 0x468c9fffu, 2}}};
    update_drive_frames(o);
}

void State::disable_hand() {
    if (selected_hand_424 != 2) {
        hands[selected_hand_424].dynamics = {{{0, 0, 0, 2}, {0, 0, 0, 2}}};
        selected_hand_424 = 2;
    }
}

void State::let_go(SkateboardControllerFields& fields, const Observation& o, const Settings& s, Effects& e) {
    e.disable_animation();
    e.released_board();
    e.collision_volumes(true);
    e.clear_alignment();
    disable_hand();
    if (fields.state_448 == 4) {
        e.velocity(Vector{});
    }
    fields.word_444 = 0;
    if ((o.processed.flags_2476 & 0x1000u) != 0) {
        fields.word_444 = 8;
        e.velocity(motion::throw_velocity(o.processed, s));
    }
}

void State::hide(const Observation& o, Effects& e) {
    e.collision_volumes(false);
    e.disable_linear_drive();
    e.clear_alignment();
    disable_hand();
    retrieval = Retrieval{};
    retrieval.initial_0 = o.processed.board_frame_64;
    retrieval.elapsed_192 = 0.0f;
    e.enable_animation_angular_only();
}

void State::retrieve(const SkateboardControllerFields& fields, const Observation& o, const Settings& s, Effects& e) {
    e.collision_volumes(false);
    e.clear_alignment();
    e.enable_animation_angular_only();
    const Vector old = retrieval.initial_0[3];
    retrieval = Retrieval{};
    retrieval.initial_0 = o.processed.board_frame_64;
    if (fields.state_448 == 3) {
        retrieval.initial_0[3] = old;
    }
    retrieval.target_64 = o.attachment_frame_0;
    Vector delta = sub(retrieval.initial_0[3], retrieval.target_64[3]);
    float distance = length(delta);
    const float tiny = rs::from_bits(0x3a83126fu);
    if (!(distance >= tiny)) {
        distance = tiny;
        delta = scale(o.processed.player_frame_192[2], tiny);
    }
    const bool mounting = (o.processed.flags_2480 & 0x80000u) != 0;
    const float maximum = mounting ? s.mounted_return_distance : s.return_distance;
    if (distance > maximum || fields.state_448 == 3) {
        retrieval.initial_0[3] = madd(scale(delta, maximum), reciprocal(distance, 2), retrieval.target_64[3]);
        e.position(retrieval.initial_0[3]);
        distance = maximum;
    }
    retrieval.duration_196 = s.retrieval_time.evaluate(distance);
    retrieval.elapsed_192 = 0.0f;
    if (mounting) {
        const float d = retrieval.duration_196;
        retrieval.duration_196 = s.mounting_time - d >= 0.0f ? d : s.mounting_time;
    }
}

void State::update_state(SkateboardControllerFields& f, const Observation& o, const Settings& s, Effects& e) {
    const Processed& p = o.processed;
    switch (f.state_448) {
        case 1: {
            const bool free_allowed = (p.flags_2480 & 0x80000u) == 0 && (p.flags_2488 & 0x8000000u) == 0;
            if ((p.flags_2476 & 0x2000u) != 0 || (free_allowed && (o.board_collision_flags_872 & 0x4000000u) != 0)) {
                f.word_444 = 0;
                let_go(f, o, s, e);
                f.state_448 = 2;
            } else {
                const std::size_t hand = selected_hand_424 == 0 ? 0 : 1;
                Alignment a;
                a.first_1008 = unit(sub(o.physical_hand_positions[hand], p.board_frame_64[3]), Vector{});
                a.second_1024 = unit(sub(p.position_592, p.board_frame_64[3]), Vector{});
                a.factor_1040 = rs::from_bits(0x3f4ccccdu);
                a.flag_1044 = true;
                e.alignment(a);
            }
            break;
        }
        case 2: {
            const Vector delta = sub(p.board_frame_64[3], p.position_592);
            const bool distant = dot(delta, delta) > s.hide_distance * s.hide_distance;
            if ((p.flags_2480 & 0x80000u) != 0 || (p.flags_2476 & 0x800u) != 0) {
                f.word_444 = 0;
                retrieve(f, o, s, e);
                f.state_448 = 4;
            } else if (distant || o.board_state_840 == 6) {
                f.word_444 = 0;
                hide(o, e);
                f.state_448 = 3;
            }
            break;
        }
        case 3:
            if ((p.flags_2480 & 0x80000u) != 0 || (p.flags_2476 & 0x800u) != 0) {
                f.word_444 = 0;
                retrieve(f, o, s, e);
                f.state_448 = 4;
            }
            break;
        case 4: {
            const std::size_t hand = (~p.flags_2476 >> 2) & 1u;
            if ((p.flags_2480 & 0x80000u) == 0 && o.hand_contacts[hand]) {
                f.word_444 = 0;
                let_go(f, o, s, e);
                f.state_448 = 2;
            } else if (!(retrieval.progress_200 < 1.0f)) {
                e.velocity(Vector{});
                f.word_444 = 0;
                hold(f, o, e);
                f.state_448 = 1;
            }
            break;
        }
        case 5:
            if ((o.board_collision_flags_872 & 0x4000000u) != 0) {
                f.word_444 = 0;
                let_go(f, o, s, e);
                f.state_448 = 2;
            } else {
                Alignment a;
                a.first_1008 = p.board_frame_64[2];
                a.second_1024 = p.board_frame_64[2];
                a.factor_1040 = rs::from_bits(0x3f733333u);
                a.flag_1044 = false;
                e.alignment(a);
            }
            break;
        default:
            break;
    }
}

// ------------------------------------------------------------------ motion.rs
static const float DT = rs::from_bits(0x3c888889u);
static const float RADIANS = rs::from_bits(0x3c8efa35u);

void State::update_drive_frames(const Observation& o) {
    const std::size_t hand = selected_hand_424 == 0 ? 0 : 1;
    const Frame a = o.animation_board_frame_12624;
    const Frame b = o.animation_hand_frames[hand];
    Frame inverse{};
    for (std::size_t column = 0; column < 4; ++column) {
        inverse[column] = column < 3 ? Vector{a[0][column], a[1][column], a[2][column], 0.0f} : Vector{};
    }
    const Vector negative = sub(Vector{}, a[3]);
    const Vector translation =
        madd(inverse[0], negative[0], madd(inverse[1], negative[1], scale(inverse[2], negative[2])));
    Frame parent = IDENTITY;
    for (std::size_t i = 0; i < 3; ++i) {
        parent[i] = transform_direction(inverse, b[i]);
    }
    parent[3] = madd(inverse[2], b[3][2], madd(inverse[1], b[3][1], madd(inverse[0], b[3][0], translation)));
    hands[selected_hand_424].child = IDENTITY;
    hands[selected_hand_424].parent = parent;
}

void State::update(SkateboardControllerFields& f, const Observation& o, const Settings& s, Effects& e) {
    if (!f.system_on_452) {
        return;
    }
    update_state(f, o, s, e);
    switch (f.state_448) {
        case 1:
            update_drive_frames(o);
            break;
        case 2:
            if (static_cast<std::int32_t>(f.word_444) > 0) {
                f.word_444 = f.word_444 - 1u;
                for (const Vector& torque : motion::throw_torques(o.processed, s)) {
                    e.torque(torque);
                }
            }
            break;
        case 3:
            retrieval.elapsed_192 += DT;
            retrieval.progress_200 = 0.5f;
            retrieval.weight_204 = 0.5f;
            retrieval.target_64 = retrieval.initial_0;
            retrieval.target_64[3] =
                madd(o.processed.hide_direction_464, s.hide_offset, o.processed.player_frame_192[3]);
            retrieval.current_128 = retrieval.target_64;
            e.hook_frame(retrieval.current_128);
            e.position(retrieval.current_128[3]);
            e.velocity(Vector{});
            break;
        case 4: {
            retrieval.elapsed_192 += DT;
            const float fraction = retrieval.elapsed_192 / retrieval.duration_196;
            retrieval.progress_200 = 1.0f - fraction >= 0.0f ? fraction : 1.0f;
            retrieval.weight_204 = s.retrieval_weight.evaluate(retrieval.progress_200);
            retrieval.target_64 = o.attachment_frame_0;
            retrieval.current_128 =
                animation::foot_ik::interpolate_affine(retrieval.initial_0, retrieval.target_64, retrieval.weight_204);
            e.hook_frame(retrieval.current_128);
            e.target_position_velocity(retrieval.current_128[3]);
            break;
        }
        default:
            break;
    }
}

namespace motion {

Vector throw_velocity(const Processed& p, const Settings& s) {
    const auto [sin0, cos0] = trigonometry::sin_cos(0.0f * RADIANS);
    const Frame yaw{{{cos0, 0.0f, -sin0, cos0}, {0.0f, 1.0f, 0.0f, 0.0f}, {sin0, 0.0f, cos0, sin0}, {}}};
    const auto [sin, cos] = trigonometry::sin_cos(-s.throw_pitch * RADIANS);
    const Vector local = transform_direction(yaw, Vector{0.0f, -sin, cos, 0.0f});
    const Vector direction = transform_direction(p.player_frame_192, local);
    const float speed = length(p.velocity_912);
    return scale(direction, speed + s.throw_velocity.evaluate(speed));
}

std::array<Vector, 3> throw_torques(const Processed& p, const Settings& s) {
    Vector flat_board = p.board_frame_64[2];
    flat_board[1] = 0.0f;
    Vector flat_direction = p.direction_400;
    flat_direction[1] = 0.0f;
    const Vector heading = unit(flat_direction, unit(flat_board, Vector{0.0f, 0.0f, 1.0f, 0.0f}));
    const Vector up{0.0f, 1.0f, 0.0f, 0.0f};
    const Vector right0 = cross(up, heading);
    const Frame basis{right0, up, heading, Vector{}};
    const auto [sin, cos] = trigonometry::sin_cos(-s.throw_target_pitch * RADIANS);
    const Vector target_right = transform_direction(basis, Vector{1.0f, 0.0f, 0.0f, 1.0f});
    const Vector target_up = transform_direction(basis, Vector{0.0f, cos, sin, 0.0f});
    const Vector target_forward = transform_direction(basis, Vector{0.0f, -sin, cos, 0.0f});
    Vector forward = p.board_frame_64[2];
    Vector right = p.board_frame_64[0];
    if (0.0f > dot(forward, target_forward)) {
        forward = rs::map(forward, [](float v) { return -v; });
        right = rs::map(right, [](float v) { return -v; });
    }
    return {
        scale(scale(target_up, wrap(projected_angle(forward, target_forward, target_up))), s.throw_yaw_scalar),
        scale(scale(target_right, wrap(projected_angle(forward, target_forward, target_right))),
              s.throw_pitch_scalar),
        scale(scale(target_forward, wrap(projected_angle(right, target_right, target_forward))),
              s.throw_roll_scalar),
    };
}

}  // namespace motion

}  // namespace s3::player::offboard::board_possession
