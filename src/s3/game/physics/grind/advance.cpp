// Port of skate-game/src/physics/grind/advance.rs.
#include "s3/game/physics/grind/advance.hpp"

#include "s3/core/physics/grind_forces/reckoning.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/grind.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::grind::advance_ {

using V = std::array<float, 4>;
using arithmetic::dot3;

namespace {

void reckon(GamePhysics& physics, SkaterRuntime& skater, V normal, V heading, float smoothing) {
    ::s3::physics::grind_forces::reckoning::update(
        physics.riding.reckoning, physics.riding.reckoning_frames, physics.riding.body_spin,
        skater.air_reckoning.state, skater.grind.settings.reckoning, normal, heading, smoothing,
        (skater.player_input.processed.flags_2468 & 0x00100000u) != 0, 0.0f);
}

V cross(V a, V b) {
    return {
        rs::mul_add(-a[2], b[1], a[1] * b[2]),
        rs::mul_add(-a[0], b[2], a[2] * b[0]),
        rs::mul_add(-a[1], b[0], a[0] * b[1]),
        0.0f,
    };
}

}  // namespace

Status advance(GamePhysics& physics, SkaterRuntime& skater) {
    if (skater.grind.nonspecific_active) {
        S3G_CHECK(substate::execute_nonspecific(physics, skater));
        S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                    "701 requires BoardToolkit");
        const auto frame = toolkit->deck;
        //82F825D0/F0 initialize830BD320/+4A0 from8231A844(+1)/8216DEE0(-1).
        const float sign = (skater.player_input.processed.flags_2484 & 0x00200000u) != 0 ? -1.0f : 1.0f;
        const V up = {frame[1][0] * sign, frame[1][1] * sign, frame[1][2] * sign, frame[1][3] * sign};
        reckon(physics, skater, up, frame[2], 0.98f);
        return ok();
    }
    S3G_TRY_OPT(const Family family, skater.grind.active, "Grind Update requires active family");
    S3G_TRY_OPT(const ManagerObservation manager, skater.grind.manager, "Grind Update requires manager observation");
    S3G_TRY_OPT(const auto* toolkit, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "Grind Update requires BoardToolkit");
    const auto frame = toolkit->deck;
    const float speed = skater.player_input.processed.scalar_2652;
    const std::size_t idx = index(family);
    {
        auto& state = skater.grind.states[idx];
        state.output.just_jumped = false;
        const bool tipped0 = state.output.tipslide_97_98_99[0];
        if (!tipped0) {
            state.output.direction = manager.geometry.direction_1136;
            state.output.normal = manager.geometry.normal_1152;
            state.output.across = cross(state.output.direction, state.output.normal);
        }
    }
    const bool tipped = skater.grind.states[idx].output.tipslide_97_98_99[0];
    const V normal = tipped ? frame[1] : manager.geometry.target_up_1168;
    const float smoothing = !tipped && manager.surface.reckon_blend_selector_1500 > 0.0f ? 0.9f : 0.7f;
    reckon(physics, skater, normal, frame[2], smoothing);
    {
        auto& state = skater.grind.states[idx];
        // Common UserTriggeredExit82D40D90 is processed2476 bit29. Slides'
        //82D41788 applies it only to ledges (kind2), preserving the native gate.
        const bool user_exit = (skater.player_input.processed.flags_2476 & 0x20000000u) != 0 &&
                               (!(family == Family::Boardslide || family == Family::Darkslide) ||
                                manager.geometry.kind_1464 == 2);
        const bool should_exit = (manager.engagement.kind_1248 == 2 &&
                                  ((manager.geometry.flags_1476 & 0x08000000u) != 0 ||
                                   (manager.control.flags_2488 & 0x10000000u) != 0) &&
                                  speed < 1.0f) ||
                                 static_cast<std::int32_t>(manager.control.flags_1516) < 0 || user_exit ||
                                 (state.updates > 60 && speed < 0.15f);
        if (state.output.substate == 1 && should_exit) {
            state.output.substate = 2;
        }
    }
    S3G_CHECK(substate::execute(physics, skater, manager));
    auto& state = skater.grind.states[idx];
    state.leaving_updates = state.output.substate == 2
                                ? static_cast<std::int32_t>(static_cast<std::uint32_t>(state.leaving_updates) + 1u)
                                : 0;
    if (state.leaving_updates > 35) {
        skater.wipeout.state.request(17, 0.0f);
    }
    state.output.crouch = rs::abs(dot3(frame[2], state.output.normal)) * 0.4f;
    const auto& p = skater.player_input.processed;
    skater.ground.steering.update(0.0f, skater.air_settings.steering_blend, p.flags_2468, p.flags_2472);
    const V velocity = rs::from_bits(p.vectors_400_416[0]);
    for (std::size_t i = 0; i < 4; ++i) {
        skater.skeleton_input.head_tracking_history[5][i] =
            rs::mul_add(velocity[i], skater.grind.settings.look_ahead, manager.geometry.point_1120[i]);
    }
    //82D3F614 sets r4=1; steering leaf preserves it to the82D3F6E0 store.
    skater.skeleton_input.head_tracking_active = true;
    state.updates = static_cast<std::int32_t>(static_cast<std::uint32_t>(state.updates) + 1u);
    return ok();
}

}  // namespace s3::game::physics::grind::advance_
