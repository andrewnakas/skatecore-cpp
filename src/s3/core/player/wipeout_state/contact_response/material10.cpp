// Port of skate-core/src/player/wipeout_state/contact_response/material10.rs (whole file).
#include "s3/core/player/wipeout_state/contact_response.hpp"

namespace s3::player::wipeout_state::contact_response::material10 {

using namespace s3::player::wipeout_state::math;

namespace {
const float STEP = rs::from_bits(0x3c888889u);            // 820849C8
const float PHASE_DURATION = rs::from_bits(0x3d072b02u);  // 8208F5C0:0.033
}  // namespace

void Response::update(SkeletonBody& body, V velocity, std::optional<V> contact) {
    if (contact) {
        normal = *contact;
    }
    finished = false;
    select(contact.has_value());
    switch (phase) {
        case 0: {
            const V difference = sub(velocity, previous_velocity);
            // Both original branches require the ordered greater-than bit.
            if (time > rs::from_bits(0x3c75c28fu) && dot(difference, difference) > 1.0f) {
                previous_velocity = madd(velocity, rs::from_bits(0x3d4ccccdu),
                                         scale(previous_velocity, rs::from_bits(0x3f733333u)));
            } else {
                previous_velocity = velocity;
            }
            break;
        }
        case 1: {
            // 91A40: half-opposite COM velocity, conditional plane rejection,
            // then an additional -normal term. This is a velocity delta.
            const V rejected = reject_positive(scale(velocity, -0.5f), normal);
            apply_velocity_delta(body, madd(normal, -1.0f, rejected));
            break;
        }
        case 2: {
            // 91B28 preserves the Processed608 snapshot used by phase1.
            apply_velocity_delta(body, scale(sub(target_velocity, velocity), 1.0f));
            // Global830BD330 initializer82F826B0 loads -9.8 from822F8B40.
            target_velocity = madd(V{0.0f, rs::from_bits(0xc11ccccdu), 0.0f, 0.0f}, STEP, target_velocity);
            break;
        }
        default:
            break;
    }
    time += STEP;
}

void Response::select(bool contact) {
    std::uint32_t next = phase;
    if (phase == 0 && contact) {
        next = -dot(previous_velocity, normal) > 2.0f ? 1u : 4u;
    } else if (phase == 1 && time > PHASE_DURATION) {
        const V twice_normal = scale(normal, 2.0f);
        target_velocity = scale(sub(previous_velocity, scale(twice_normal, dot(previous_velocity, normal))),
                                rs::from_bits(0x3f666666u));
        next = 2;
    } else if (phase == 2 && time > PHASE_DURATION) {
        finished = true;
        next = 3;
    } else if (phase == 3 && !contact) {
        next = 0;
    }
    // Native unsigned phase>3 bypasses transitions, including phase4.
    if (next != phase) {
        phase = next;
        time = 0.0f;
    }
}

}  // namespace s3::player::wipeout_state::contact_response::material10
