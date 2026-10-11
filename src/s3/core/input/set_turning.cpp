// Port of skate-core/src/input/set_turning.rs.
// SetTurning enter 82BB3810 and update 82BB3830 at resolved component boundaries.
#include "s3/core/input/set_turning.hpp"

namespace s3::input::set_turning {

using rs::mul_add;

void update(State& state, SlideLatch& latch_owner, Physical physical, std::pair<bool, bool> stance, float dt,
            const Settings& settings, Intents intents, const std::function<void(Attribute, float)>& emit) {
    auto& latch = latch_owner.words;
    const auto a = settings.remaps[0].apply({physical.field_32, physical.field_56});
    (void)settings.remaps[0].apply({physical.field_36, physical.field_56});
    const auto b = settings.remaps[1].apply({physical.field_32, physical.field_56});
    (void)settings.remaps[1].apply({physical.field_36, physical.field_56});
    const float complement = 1.0f - physical.field_52;
    float target = -mul_add(a[0], physical.field_52, b[0] * complement);
    const float direction = -mul_add(a[1], physical.field_52, b[1] * complement);
    const std::uint32_t old_0 = latch[1];
    const std::uint32_t old_1 = latch[3];
    latch[1] &= ~0x20000000u;
    latch[3] &= ~0x20000000u;
    const bool prior_0 = (old_0 & 0x20000000u) != 0;
    const bool prior_1 = (old_1 & 0x20000000u) != 0;
    const bool enabled = (latch[4] & 0x40000000u) != 0;
    std::uint32_t mode = state.mode;
    if (mode == 2) {
        const bool first = enabled && ((latch[3] & 0x80000000u) != 0 || prior_1);
        const bool second = enabled && ((latch[1] & 0x80000000u) != 0 || prior_0);
        if (first || second) {
            if (!prior_0 && !prior_1) {
                latch[0] = 0;
                latch[2] = 0;
                latch[4] = (latch[4] & 0x7fffffffu) | (std::uint32_t(stance.first) << 31);
            }
            mode = first ? 1 : 0;
        }
    } else if ((mode == 1 && (!enabled || (latch[3] & 0x40000000u) != 0)) ||
               (mode == 0 && (!enabled || (latch[1] & 0x40000000u) != 0))) {
        mode = 2;
    }
    if (mode <= 1) {
        target = ((mode == 1) ^ stance.second) ? 1.0f : -1.0f;
        if ((latch[4] & 0x80000000u) != 0) {
            target = -target;
        }
    }
    if (mode != state.mode) {
        state.elapsed = 0.0f;
    }
    state.mode = mode;
    const float blend = settings.blend.evaluate(state.elapsed);
    const float lower = state.smoothed - settings.maximum_delta;
    const float upper = state.smoothed + settings.maximum_delta;
    float value = mul_add(1.0f - blend, state.smoothed, blend * target);
    value = lower - value >= 0.0f ? lower : value;
    state.elapsed += dt;
    state.smoothed = upper - value >= 0.0f ? value : upper;
    const float speed = rs::abs(physical.body_168);
    const float tuck =
        speed > settings.speed_threshold ? settings.speed_tuck.evaluate(speed - settings.speed_threshold) : 0.0f;
    emit(Attribute::Angle, state.smoothed);
    emit(Attribute::Direction, direction);
    emit(Attribute::Quickness, physical.field_52);
    emit(Attribute::Speed, tuck);
    emit(Attribute::Holding, physical.field_60);
    if (mode <= 1) {
        const bool flip = (latch[4] & 0x80000000u) != 0;
        const float turn = ((mode == 1) ^ flip) ? settings.override_turn : -settings.override_turn;
        emit(Attribute::Turn, turn);
        const std::optional<float> chosen = mode == 0 ? intents.mode_0_slide : intents.mode_1_slide;
        float slide = chosen ? *chosen : 0.0f;
        if (flip) {
            const float negative = -slide;
            const float absolute = rs::abs(negative);
            const float v = absolute > 0.5f ? 1.0f - absolute : absolute;
            slide = negative >= 0.0f ? v : -v;
        }
        emit(Attribute::Slide, slide);
    } else if (intents.fakie_turn) {
        emit(Attribute::Turn, *intents.fakie_turn);
    }
}

}  // namespace s3::input::set_turning
