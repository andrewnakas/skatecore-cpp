// Port of skate-core/src/animation/foot_ik/status.rs.
#include "s3/core/animation/foot_ik/status.hpp"

namespace s3::animation::foot_ik::status {

namespace {
float select(float condition, float nonnegative, float negative) {
    return condition >= 0.0f ? nonnegative : negative;
}
float positive(float value) { return select(-value, 0.0f, value); }
void approach(float& value, float target, float step) {
    float delta = target - value;
    delta = select(-step - delta, -step, delta);
    value = select(step - delta, delta, step) + value;
}
}  // namespace

void update_modes(std::array<LimbStatus, 4>& limbs, bool feet_enabled, std::uint32_t flags_2472) {
    const std::array<bool, 4> enabled = {
        feet_enabled,
        feet_enabled,
        (flags_2472 & 0x80u) != 0,
        (flags_2472 & 0x100u) != 0,
    };
    for (std::size_t i = 0; i < 4; ++i) {
        LimbStatus& limb = limbs[i];
        const Mode fallback = enabled[i] ? Mode::OnDeck : Mode::Disabled;
        switch (limb.mode) {
            case Mode::Disabled:
                limb.board_blend = 0.0f;
                if (limb.external_target_set) {
                    limb.mode = Mode::External;
                    limb.external_blend = limb.target_blend;
                } else if (limb.local_target_set) {
                    limb.mode = Mode::Local;
                    limb.external_blend = limb.target_blend;
                    limb.external_target_local_delta = {0.0f, 0.0f, 0.0f, 0.0f};
                } else {
                    limb.mode = fallback;
                    limb.external_blend = 0.0f;
                }
                break;
            case Mode::OnDeck:
                if (limb.external_target_set) {
                    limb.mode = Mode::External;
                    limb.external_blend = limb.target_blend;
                } else if (limb.local_target_set) {
                    limb.mode = Mode::Local;
                    limb.external_blend = limb.target_blend;
                    limb.external_target_local_delta = {0.0f, 0.0f, 0.0f, 0.0f};
                } else if (limb.board_blend == 0.0f) {
                    limb.mode = fallback;
                    limb.external_blend = 0.0f;
                }
                break;
            case Mode::External:
                if (!limb.external_target_set) {
                    limb.mode = Mode::Local;
                }
                break;
            case Mode::Local:
                if (limb.external_target_set) {
                    limb.mode = Mode::External;
                    limb.external_blend = limb.target_blend;
                }
                // This is a second independent test, even after switching to
                //External above. Preserve the native zero-target-blend case.
                if (!limb.local_target_set && limb.external_blend == 0.0f) {
                    limb.mode = fallback;
                    limb.external_target_local_delta = {0.0f, 0.0f, 0.0f, 0.0f};
                }
                break;
        }
    }
}

void update_blends(std::array<LimbStatus, 4>& limbs, float deck_half_width, float deck_total_half_length,
                   std::uint32_t flags_2480, const BlendSettings& settings) {
    const std::array<float, 4> base = {deck_half_width, 0.0f, deck_total_half_length, 0.0f};
    std::array<float, 4> inner{};
    std::array<float, 4> outer{};
    for (std::size_t i = 0; i < 4; ++i) inner[i] = base[i] + settings.hand_inner_padding[i];
    for (std::size_t i = 0; i < 4; ++i) outer[i] = inner[i] + settings.hand_outer_padding[i];
    const float step = settings.board_blend_step;
    for (std::size_t index = 0; index < 4; ++index) {
        LimbStatus& limb = limbs[index];
        switch (limb.mode) {
            case Mode::Disabled: break;
            case Mode::External:
                limb.external_blend = limb.target_blend;
                limb.board_blend = positive(limb.board_blend - step);
                break;
            case Mode::Local:
                limb.external_blend = limb.local_target_set
                                          ? 1.0f
                                          : positive(limb.external_blend - settings.external_blend_step);
                limb.board_blend = positive(limb.board_blend - step);
                break;
            case Mode::OnDeck:
                if (index < 2) {
                    const float target = (flags_2480 & 0x08000000u) != 0 ? 0.0f : 1.0f;
                    approach(limb.board_blend, target, step);
                } else {
                    std::array<float, 4> p{};
                    for (std::size_t i = 0; i < 4; ++i) p[i] = rs::abs(limb.part_position[i]);
                    if (limb.board_blend > 0.0f || (p[0] < outer[0] && p[1] < outer[1] && p[2] < outer[2])) {
                        float target = 1.0f;
                        // Native order X, Z, Y; fsel preserves equality and NaNs.
                        for (std::size_t axis : {std::size_t(0), std::size_t(2), std::size_t(1)}) {
                            if (p[axis] > inner[axis]) {
                                const float candidate =
                                    1.0f - (p[axis] - inner[axis]) / settings.hand_outer_padding[axis];
                                if (axis == 0) {
                                    target = candidate;
                                } else if (axis == 2) {
                                    target = select(target - candidate, candidate, target);
                                } else {
                                    target = select(candidate - target, target, candidate);
                                }
                            }
                        }
                        target = positive(target);
                        target = select(1.0f - target, target, 1.0f);
                        approach(limb.board_blend, target, step);
                    }
                }
                break;
        }
    }
}

}  // namespace s3::animation::foot_ik::status
