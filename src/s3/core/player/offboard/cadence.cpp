// Port of skate-core/src/player/offboard/cadence.rs (whole file).
#include "s3/core/player/offboard/cadence.hpp"

namespace s3::player::offboard::cadence {

namespace {
const float DT = rs::from_bits(0x3c888889u);
const float EPSILON = rs::from_bits(0x3a83126fu);

float select(float test, float positive, float negative) { return test >= 0.0f ? positive : negative; }
float clamp(float value, float lower, float upper) {
    value = select(lower - value, lower, value);
    return select(upper - value, value, upper);
}
Vector subtract(Vector a, Vector b) { return Vector{a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
float dot(Vector a, Vector b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vector reject(Vector vector, Vector axis) {
    const float projection = dot(axis, vector);
    return Vector{vector[0] - axis[0] * projection, vector[1] - axis[1] * projection,
                  vector[2] - axis[2] * projection};
}
float square_root(float squared) {
    // Independent PC seed. Preserve both original refinement steps and zero select.
    float inverse = rs::recip(rs::sqrt(squared));
    for (int i = 0; i < 2; ++i) {
        inverse = rs::mul_add(inverse * 0.5f, rs::mul_add(-squared, inverse * inverse, 1.0f), inverse);
    }
    const float result = squared * inverse;
    return squared == 0.0f ? 0.0f : result;
}
float length(Vector vector) { return square_root(dot(vector, vector)); }
}  // namespace

void BipedCadence::update(const CadenceInput& input, CadenceThresholds thresholds) {
    Vector motion = subtract(input.motion_512, input.motion_reference_272);
    if (input.reject_enabled_708) {
        motion = reject(motion, input.reject_axis_400);
    }
    float magnitude = length(reject(motion, input.up_144));
    for (std::uint32_t index = 0; index < 4; ++index) {
        if (magnitude <= thresholds._0[index]) {
            locomotion_index = index;
            break;
        }
    }
    if (!(input.requested_phase_296 < 0.0f)) {
        phase.request(input.requested_phase_296, input.requested_duration_288);
    } else if (locomotion_index == 0) {
        if (!(phase.target >= 0.0f)) {
            phase.stop();
        }
    } else {
        const Vector animation = input.animation_motion_224;
        const Vector horizontal{animation[0], 0.0f, animation[2]};
        const float horizontal_length = length(horizontal);
        if (horizontal_length > EPSILON) {
            // 82D80A58..AA8 transposes rows, then accumulates x, y, z.
            const Vector local = rs::map(input.frame_rows_0_16_32, [&](const Vector& row) {
                return rs::mul_add(row[2], motion[2], rs::mul_add(row[1], motion[1], row[0] * motion[0]));
            });
            const float projection = (1.0f / horizontal_length) * dot(local, horizontal);
            magnitude = square_root(rs::mul_add(local[1], local[1], projection * projection));
        }
        const float animation_length = length(animation);
        phase.rate = animation_length < EPSILON ? 0.0f : magnitude / animation_length;
        phase.target = -1.0f;
        if (!input.reject_enabled_708 && !input.suppress_adjustment_353 && (input.contact_flags_176 & 0x180u) != 0) {
            const float time =
                (length(subtract(input.contact_point_96, input.frame_position_48)) - 0.2f) / magnitude;
            const float lower = phase.rate * 0.4f;
            const float upper = phase.rate * 2.5f;
            if (time > rs::from_bits(0x3c088889u)) {
                if ((input.contact_flags_176 & 0x100u) != 0) {
                    const float target = (phase.phase > 0.37f && phase.phase <= 0.87f) ? 0.9f : 0.4f;
                    phase.adjust_targets(target, target, time, upper);
                } else {
                    phase.adjust_contact(time, lower, upper);
                }
            }
        }
    }
    phase.advance();
}

void BipedPhase::request(float target_, float duration_) {
    if (duration_ > 0.1f) {
        duration_ -= 0.1f;
    }
    if (target == target_ && duration.has_value() && *duration == duration_) {
        return;
    }
    const float delta = target_ - phase;
    target = target_;
    duration = duration_;
    forward_target = true;
    rate = delta < 0.0f ? (delta + 1.0f) / duration_ : delta / duration_;
}

void BipedPhase::stop() {
    float target_ = 0.0f;
    float best = rs::from_bits(0x7149f2cau);
    for (float candidate : {0.0f, 0.5f}) {
        const float raw = candidate - phase;
        const float wrapped = wrap(0.0f, raw + 1.0f, 1.0f);
        const float distance = rs::abs(raw) < rs::abs(wrapped) ? rs::abs(raw) : rs::abs(wrapped);
        if (distance < best) {
            target_ = candidate;
            best = distance;
        }
    }
    forward_target = false;
    duration = 0.2f;
    target = target_;
    const float distance = rs::abs(target_ - phase);
    rate = select(distance - (1.0f - distance), 1.0f - distance, distance) * 5.0f;
}

void BipedPhase::advance() {
    if (target < 0.0f) {
        const float next = rs::mul_add(rate, DT, phase);
        phase = next - rs::floor(next);
    } else if (forward_target) {
        if (phase == target) {
            return;
        }
        const float goal = target < phase ? target + 1.0f : target;
        const float old = phase;
        const float next = rs::mul_add(rate, DT, old);
        phase = (old < goal && next >= goal) ? target : next - rs::floor(next);
    } else {
        const float old = phase;
        float next = old + wrap(-0.5f, target - old, 0.5f);
        const float step = rate * DT;
        if (old > next + step) {
            next = old - step;
        } else if (old < next - step) {
            next = old + step;
        }
        phase = wrap(0.0f, next, 1.0f);
    }
}

void BipedPhase::adjust_targets(float a, float b, float time, float upper) {
    const float da = wrap(0.0f, a - phase, 1.0f);
    const float db = wrap(0.0f, b - phase, 1.0f);
    if (da >= 1.0f) {
        rate = clamp(db / time, 0.0f, upper);
    } else if (db >= 1.0f) {
        rate = clamp(da / time, 0.0f, upper);
    } else {
        const float inverse = 1.0f / time;
        const float ra = da * inverse;
        const float rb = db * inverse;
        rate = select_rate(ra, rb, 0.0f, upper, select(ra - rb, rb, ra));
    }
}

void BipedPhase::adjust_contact(float time, float lower, float upper) {
    const float inverse = 1.0f / time;
    const float a = wrap(0.0f, 0.45f - phase, 1.0f) * inverse;
    const float b = wrap(0.0f, 0.95f - phase, 1.0f) * inverse;
    const float preferred = rs::abs(rate - a) < rs::abs(rate - b) ? a : b;
    rate = select_rate(a, b, lower, upper, preferred);
}

float select_rate(float a, float b, float lower, float upper, float both) {
    if (a >= lower && a <= upper) {
        return (b >= lower && b <= upper) ? both : a;
    } else if (b >= lower && b <= upper) {
        return b;
    }
    const float da = a < lower ? lower - a : a - upper;
    const float db = b < lower ? lower - b : b - upper;
    return clamp(da < db ? a : b, lower, upper);
}

float wrap(float lower, float value, float upper) {
    std::int32_t count;
    if (value < lower) {
        count = rs::as_i32(rs::floor((upper - value) / (upper - lower)));
    } else if (value > upper) {
        count = static_cast<std::int32_t>(
            0u - static_cast<std::uint32_t>(rs::as_i32(rs::floor((value - lower) / (upper - lower)))));
    } else {
        count = 0;
    }
    return clamp(rs::mul_add(upper - lower, static_cast<float>(count), value), lower, upper);
}

}  // namespace s3::player::offboard::cadence
