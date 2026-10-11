// Port of skate-core/src/player/offboard/air_launch.rs and air_launch/jump.rs.
#include "s3/core/player/offboard/air_launch.hpp"

#include "s3/core/player/offboard/contact_correction.hpp"
#include "s3/core/player/wipeout_state/orientation.hpp"

namespace s3::player::offboard::air_launch {

using namespace math;

std::uint8_t mode(const Processed& p, bool current) {
    const std::uint32_t state = current ? p.current_state_2508 : p.previous_state_2504;
    const std::uint32_t category = current ? p.current_category_2512 : p.previous_category_2516;
    switch (category) {
        case 100: return 0;
        case 400: return 1;
        case 200: return 2;
        case 500:
            if ((p.flags_2480 & 0x80u) != 0 || state == 503) return 3;
            if ((p.flags_2476 & 0x80000u) != 0) return 4;
            return 5;
        default: return 6;
    }
}

rs::Result<rs::Unit, const char*> produce(Packet& packet, const controller::State& state,
                                          const PointGraph<8>& turn_vs_speed, Settings settings, const Processed& p,
                                          bool current) {
    const std::uint8_t selected_mode = mode(p, current);
    // Validate the host contract before ANY packet field changes. No numerical
    // fallback or inferred geometry is substituted for an unavailable producer.
    std::optional<DepartureGeometry> departure;
    if (selected_mode == 1) {
        if (!p.departure_geometry) {
            return rs::make_err("Offboard launch mode1 requires source-backed departure geometry1120/1136");
        }
        departure = p.departure_geometry;
    }
    const Vector forward = normalize_or(flatten(p.forward_224), ZERO);
    packet.up_48 = p.up_544;
    packet.forward_64 = forward;
    packet.scalar_96 = rs::from_bits(0x3db2b8c2u);
    packet.scalar_100 = rs::from_bits(0x3f5f66f3u);
    packet.scalar_104 = rs::from_bits(0x3f32b8c2u);
    packet.kind_108 = 1;
    packet.kind_112 = 0;
    packet.position_32 = p.position_592;
    Vector velocity = p.velocity_608;
    Vector secondary = velocity;
    switch (selected_mode) {
        case 0: {
            // BC84..BCC0 is a boost, not a conventional [-3,3] clamp.
            const float lower = select(velocity[1] - 3.0f, velocity[1], 3.0f);
            velocity[1] = select((velocity[1] + 3.0f) - lower, lower, velocity[1] + 3.0f);
            packet.board_position_80 = p.board_position_112;
            packet.has_board_position_116 = true;
            packet.position_32 = madd(velocity, DT, packet.position_32);
            break;
        }
        case 1: {
            const Vector delta = sub(p.position_592, departure->point_1120);
            const Vector projected = sub(delta, scale(departure->axis_1136, dot(delta, departure->axis_1136)));
            const Vector normal = normalize_or(flatten(projected), ZERO);
            velocity = add(madd(normal, 2.0f, velocity), UP);
            packet.position_32 = madd(velocity, DT, packet.position_32);
            packet.kind_108 = 6;
            break;
        }
        case 3: {
            const Vector side = cross(UP, velocity);
            const float speed = length(side);
            if (!(speed <= rs::from_bits(0x3a83126fu))) {
                const float offset = (p.flags_2476 & 4u) != 0 ? 0.75f : -0.75f;
                velocity = madd(side, offset / speed, velocity);
            }
            break;
        }
        case 4: {
            auto r = jump::calculate(state, turn_vs_speed, settings, p, forward);
            velocity = r.first;
            secondary = r.second;
            packet.kind_108 = 6;
            packet.kind_112 = 3;
            break;
        }
        case 5: {
            const Vector horizontal = flatten(velocity);
            if (!(length(horizontal) >= 1.875f)) {
                velocity = scale(normalize_or(horizontal, forward), 2.5f);
                velocity[1] = select(1.0f - velocity[1], 1.0f, velocity[1]);
                packet.scalar_96 = rs::from_bits(0x3f060a92u);
            } else {
                velocity = scale(p.velocity_912, 0.75f);
                velocity[1] = clamp(velocity[1], -10.0f, 5.0f);
            }
            packet.kind_108 = 6;
            break;
        }
        case 2:
        case 6:
            break;
        default:
            S3_PANIC("mode returns only the seven native cases");
    }
    packet.velocity_0 = velocity;
    packet.secondary_velocity_16 = secondary;
    return rs::Unit{};
}

namespace jump {

std::pair<Vector, Vector> calculate(const controller::State& state, const PointGraph<8>& turn, Settings settings,
                                    const Processed& p, Vector forward) {
    using wipeout_state::orientation::projected_angle;
    const Vector reference_up = state.frame_output.frame[1];  // Biped144
    // Preserve the explicit zero multiply and fallback to the original reference.
    const Vector blend = madd(reference_up, 1.0f, scale(state.motion.frame_0[1], 0.0f));
    const Vector up = normalize_or(blend, reference_up);
    const Vector planar = sub(p.velocity_912, scale(up, dot(up, p.velocity_912)));
    const float backward = dot(planar, forward);
    const float amount = select(-backward, 0.0f, backward) - backward;
    Vector base = madd(forward, amount, planar);
    Vector secondary = base;
    const float speed_squared = dot(base, base);
    if ((p.flags_2472 & 0x10000000u) == 0) {
        const Vector control{p.raw_x_2692, 0.0f, p.raw_z_2688, 0.0f};
        if (!(speed_squared >= 1.0f)) {
            base = contact_correction::clamp_length(madd(control, 1.0f, base), 1.0f);
            secondary = base;
        } else if (state.contact.active && dot(control, control) > rs::from_bits(0x3f4f5c28u)) {
            const float angle = wrap_angle(projected_angle(control, base, up));
            if (45.0f * RADIANS > angle) {
                secondary = scale(normalize(reject(control, up)), magnitude(speed_squared));
            } else if (90.0f * RADIANS > angle) {
                const Vector direction = normalize(reject(control, up));
                // C4E4 supplies45.0 exactly, with no radians conversion.
                secondary = scale(limit_angle(direction, base, 45.0f), magnitude(speed_squared));
            }
        }
    }
    const float launch_speed = magnitude(settings.jump_height * rs::from_bits(0x419ccccdu));
    float vertical = up[1] * launch_speed;
    vertical = select(-vertical, 0.0f, vertical);
    secondary = normalize_or(flatten(secondary), ZERO);
    const float requested_turn = (turn.evaluate(state.motion.speed_704) * state.intent.steering) * RADIANS;
    // 82D7C6C4 reads Biped+688; +672 is the unrelated edge-position vector.
    const float turn_center = state.motion.angular_velocity_688;
    const float window = rs::from_bits(0x3fdf66f3u);
    const float angle = clamp(clamp(requested_turn, turn_center - window, turn_center + window) * 0.4f,
                              rs::from_bits(0xbe860a92u), rs::from_bits(0x3e860a92u));
    const Vector impulse = madd(base, settings.jump_speed_scalar, scale(up, vertical));
    return {rotate(impulse, up, angle), secondary};
}

}  // namespace jump

}  // namespace s3::player::offboard::air_launch
