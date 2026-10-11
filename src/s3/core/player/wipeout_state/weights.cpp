// Port of skate-core/src/player/wipeout_state/weights.rs (whole file).
#include "s3/core/player/wipeout_state/weights.hpp"

#include "s3/core/player/wipeout_state/math.hpp"

namespace s3::player::wipeout_state::weights {

namespace {
float fade(float duration, float time) {
    const float divisor = duration == 0.0f ? 0.01f : duration;
    return math::clamp((duration - time) / divisor, 0.0f, 1.0f);
}
}  // namespace

Output update(State& state, Settings settings, bool contact, bool control_applied, std::uint32_t flags_2472,
              std::array<float, 2> gesture) {
    const float target_weight = fade(settings.remove_target_time, state.time);
    if (state.special_surface || (rs::abs(gesture[0]) < 0.2f && rs::abs(gesture[1]) < 0.2f)) {
        const float step = contact ? settings.collision_step : -settings.collision_step;
        state.collision_weight = math::clamp(state.collision_weight + step, 0.0f, 1.0f);
    } else {
        state.collision_weight = 0.0f;
    }
    const float drive_fade = fade(settings.remove_drives_time, state.time);
    const float drive_fade_squared = drive_fade * drive_fade;
    const bool enabled = (control_applied || (flags_2472 & 0x00080000u) != 0) && !state.special_surface;
    const float step = enabled ? settings.controlled_step : -settings.controlled_step;
    state.controlled_weight = math::clamp(state.controlled_weight + step, 0.0f, 1.0f);
    const float available = 1.0f - state.collision_weight;
    const float start = available * drive_fade_squared;
    return Output{target_weight, start, state.collision_weight, (available - start) * state.controlled_weight,
                  state.extra_weight * 0.03f};
}

}  // namespace s3::player::wipeout_state::weights
