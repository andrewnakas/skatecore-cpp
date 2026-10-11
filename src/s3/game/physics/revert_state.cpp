// Port of skate-game/src/physics/revert_state.rs.
//! RevertGround102, TU3 vtable82327330: Enter82D43488, Update82D43518,
//! empty Exit, Ground Post82D4C070, Fill82D43B10. Stock layout018E8A2E5028AB3F.
#include "s3/game/physics/revert_state.hpp"

#include "s3/core/physics/force_queue.hpp"
#include "s3/core/physics/manual/controller.hpp"
#include "s3/core/riding/anti_flip.hpp"
#include "s3/core/riding/collision_response.hpp"
#include "s3/core/riding/pumping.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/input_phase.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::revert_state {

using sc::math::Vector3;
using sc::physics::force_queue::QueuedPointForce;

rs::Result<RevertState, std::string> RevertState::load(const sd::Collections& d) {
    const std::string cls = "Hash_018E8A2E5028AB3F";
    S3G_TRY(const auto aw, d.words<20>(cls, "default", "Hash_DAA990E803A782A1"));
    S3G_TRY(const auto bw, d.words<12>(cls, "default", "Hash_3387562E9BEA0AF6"));
    const auto a = rs::from_bits(aw);
    const auto b = rs::from_bits(bw);
    RevertState out{};
    for (std::size_t i = 0; i < 8; ++i) {
        out.speed.x[i] = a[4 + i];
        out.speed.y[i] = a[12 + i];
    }
    for (std::size_t i = 0; i < 4; ++i) {
        out.tolerance.x[i] = b[4 + i];
        out.tolerance.y[i] = b[8 + i];
    }
    S3G_TRY(out.gain, d.float_(cls, "default", "Hash_0E5B939FD4ECDBAA"));
    S3G_TRY(out.delay, d.float_(cls, "default", "Hash_6BF6544475D2A5AD"));
    S3G_TRY(out.duration, d.float_(cls, "default", "Hash_035D9805EB9E2EE2"));
    out.direction = 0.0f;
    out.travel_sign = 1.0f;
    out.velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    out.elapsed = 0.0f;
    out.captured = false;
    out.active = false;
    return out;
}

V RevertState::correction(V forward, V normal, V angular) {
    if (!captured) return V{0.0f, 0.0f, 0.0f, 0.0f};
    V desired = forward;
    for (auto& v : desired) v = v * travel_sign;
    // Preserve the two native self-scaled projections82D438B4..D0.
    auto project = [&](V v) {
        const float d = dot(v, normal);
        for (auto& x : v) x = x - x * d;
        return v;
    };
    const V from = project(velocity);
    const V to = project(desired);
    if (dot(from, from) * dot(to, to) <= rs::from_bits(0x37800000u)) return V{0.0f, 0.0f, 0.0f, 0.0f};
    auto normalize = [](V v) {
        const float length = rs::sqrt(dot(v, v));
        for (auto& x : v) x = x / length;
        return v;
    };
    float angle = sc::riding::collision_response::signed_angle(xyz(normalize(from)), xyz(normalize(to)), xyz(normal));
    // 8258DB98 folds the helper's[0,2pi] output before the direction test.
    if (angle >= rs::f32::consts::PI) angle -= rs::f32::consts::TAU;
    if (angle * direction > 0.0f) angle -= direction * rs::f32::consts::TAU;
    const float target = speed.evaluate(rs::abs(angle)) * direction;
    if (rs::abs(angle) < tolerance.evaluate(normal[1]) * rs::from_bits(0x3c8efa35u) || rs::abs(angle) > 4.5f) {
        active = false;
    }
    const float current = dot(angular, normal);
    const float correction =
        (current * target > 0.0f && rs::abs(current) > rs::abs(target)) ? 0.0f : (target - current) * gain;
    V out = normal;
    for (auto& v : out) v = v * correction;
    return out;
}

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    skater.ground_lifecycle.skeleton_elapsed_16505 = true;
    physics.board.hook_mut().drive.disable_animation(skater.ground_lifecycle.board_animated_290);
    auto& r = skater.revert_state;
    r.direction = skater.animation_input.extra.revert_direction;
    r.velocity = {0.0f, 0.0f, 0.0f, 0.0f};
    r.elapsed = 0.0f;
    r.captured = false;
    r.active = true;
    log::info(fmt::cat("REVERT_ENTER tick=", physics.ticks, " direction=", fmt::debug_f32(r.direction),
                       " speed=", skater.player_input.processed.scalar_2656));
    skater.wipeout.state.enter_ground();
    return ok();
}

Status update(GamePhysics& physics, SkaterRuntime& skater) {
    {
        const auto& p = skater.player_input.processed;
        S3G_TRY_OPT(const auto* t, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                    "Revert requires current BoardToolkit");
        auto& r = skater.revert_state;
        if (!r.captured && r.elapsed >= r.delay) {
            r.captured = true;
            r.velocity = rs::from_bits(p.vectors_400_416[0]);
            r.travel_sign = dot(r.velocity, t->deck[2]) > 0.0f ? -1.0f : 1.0f;
        }
        physics.riding.update_slide_reckoning(p, *t, skater.animation_input.extra.physical_body_spin,
                                              skater.animation_input.fields.balance);
    }
    S3G_CHECK(input_phase::update_ground(physics, skater));
    const auto& p = skater.player_input.processed;
    S3G_TRY_OPT(const auto* t, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "Revert requires current BoardToolkit");
    S3G_CHECK(skater.ground.pumping_settings.update_revert(skater.ground.pumping, *t, physics.riding,
                                                           skater.animated_skeleton, p,
                                                           skater.animation_input.fields.balance));
    S3G_TRY(const auto mode, skater.ground.pumping_settings.mode(p.state_variant_index_2528));
    const auto input = skater.ground_settings->input(
        *t, p, skater.animation_input, skater.ground.pumping, mode.unintentional_scalar, physics.riding,
        skater.animated_skeleton, physics.settings.step.base_truck_transforms,
        ground_runtime::GroundInputObservations{skater.ground_lifecycle.manual_drag_2724,
                                                p.external_physics_1616.flags, 0, {0.0f, 0.0f, 0.0f, 0.0f}});
    const auto settings = skater.ground_settings->board();
    const V correction = skater.revert_state.correction(t->deck[2], rs::from_bits(p.vectors_464_480_496_512_528[0]),
                                                        rs::from_bits(p.vectors_720_784_800_816_832_864[0]));
    // 82C040F0(0): zero linear drag on every board body.
    for (auto& body : physics.board.bodies_mut()) body.inertia.linear_drag = 0.0f;
    const auto anti_flip = sc::riding::anti_flip::calculate(settings.anti_flip, input.anti_flip);
    const auto pump = sc::riding::pumping::calculate(input.pump_force);
    const auto manual_r = sc::physics::manual::controller::calculate(
        skater.ground.manual, settings.manual, settings.manual_mode, input.manual, skater.ground_runtime);
    if (manual_r.is_err()) {
        // Rust `format!("Revert manual: {e:?}")`; the measurement error type is Infallible.
        return rs::make_err(std::string("Revert manual: Angle(IntegerConversionUnavailable)"));
    }
    const auto manual = manual_r.unwrap();
    for (const V& value : {correction, manual.angular_displacement, anti_flip}) {
        skater.ground_runtime.apply_angular_displacement(physics.board, value);
    }
    physics.board.forces_mut().append(QueuedPointForce{8, Vector3::new_(pump[0], pump[1], pump[2]),
                                                       Vector3::new_(pump[4], pump[5], pump[6])});
    skater.revert_state.elapsed += p.timestep_2604;
    if (skater.revert_state.elapsed > skater.revert_state.duration) skater.revert_state.active = false;
    return ok();
}

float dot(V a, V b) { return std::fma(a[2], b[2], std::fma(a[1], b[1], a[0] * b[0])); }

Vector3 xyz(V v) { return Vector3::new_(v[0], v[1], v[2]); }

}  // namespace s3::game::physics::revert_state
