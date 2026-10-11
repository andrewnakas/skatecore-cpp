// Port of skate-game/src/physics/biped_ground/feet.rs
//! Ground-only conditioning82D77330 and history82D775D8 around the shared
//! FeetIK evaluator. Air's per-update timer resets do NOT belong here.
#include "s3/game/physics/biped_ground/feet.hpp"

namespace s3::game::physics::biped_ground::feet {

/// The host supplies actual current bone/line/root observations to core::Input.
/// This borrows state56; it never creates or retains another FeetIK owner.
void update(State& manager, core::Input input, FootIkState& ik) {
    //82D77330 only conditions when BOTH incoming line tests are valid.
    if (input.lines[0].valid && input.lines[1].valid) {
        std::array<float, 2> d{};
        for (std::size_t i = 0; i < 2; ++i) {
            d[i] = input.lines[i].position[1] - input.world_foot_pairs[i][0][1];
        }
        for (std::size_t i = 0; i < 2; ++i) {
            if (rs::abs(d[i]) > 0.3f && rs::abs(d[i] - d[1 - i]) > 0.3f) {
                input.lines[i].valid = false;
            }
        }
    }
    const auto targets = core::update(manager, input);
    //82D77558 applies IK before Ground correction/history82D775D8.
    if ((input.flags_2484 & 0x80000000u) == 0) {
        for (std::size_t i = 0; i < targets.size(); ++i) {
            const auto& target = targets[i];
            auto& limb = ik.limbs[i];
            auto& external = ik.external_targets[i];
            if (target.world) {
                external.world_position = target.position;
                limb.external_target_set = true;
            } else {
                external.animation_position = target.position;
                limb.local_target_set = true;
            }
            limb.target_blend = target.blend;
            if (target.normal) {
                core::set_normal(external, *target.normal);
            }
        }
    }
    update_history(manager);
}

void update_history(State& s) {
    //Both invalid deliberately leaves316 unchanged;312 still increments.
    bool both_bad = true;
    if (s.hands[0].flag_84 || s.hands[1].flag_84) {
        s.words_308_to_316[2] = s.words_308_to_316[2] + 1u;
        for (const auto& foot : s.hands) {
            const float height = foot.vectors_0_to_64[1][1] - foot.vectors_0_to_64[3][1];
            if (foot.flag_84 && !(height > 0.18f)) {
                s.words_308_to_316[2] = 0;
            }
            const bool bad = !foot.flag_84 || height > 1.0f || foot.vectors_0_to_64[4][1] < 0.6f;
            both_bad &= bad;
        }
    }
    if (both_bad) {
        s.words_308_to_316[1] = s.words_308_to_316[1] + 1u;
        if (static_cast<std::int32_t>(s.words_308_to_316[1]) > 20 ||
            static_cast<std::int32_t>(s.words_308_to_316[2]) > 40) {
            s.flags_304_to_307[1] = true;
        }
    } else {
        s.flags_304_to_307[1] = false;
        s.words_308_to_316[1] = 0;
    }
    s.words_308_to_316[0] = 0;
    s.flags_304_to_307[2] = false;
    s.vectors_224_to_272[0] = {0.0f, 0.0f, 0.0f, 0.0f};
    s.vectors_224_to_272[1] = {0.0f, 0.0f, 0.0f, 0.0f};
}

}  // namespace s3::game::physics::biped_ground::feet
