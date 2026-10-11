// Port of skate-game/src/physics/known_air.rs.
//! Native KnownAir201 lifecycle82D352D0/82D35C58/82D35920/82D36590/82D36880.
//! All phase adapters borrow the production owners; the state copy below is a
//! stack-local value while those owners are borrowed, then published once.
#include "s3/game/physics/known_air.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::known_air {

namespace known = ::s3::air::known;

rs::Result<KnownAir, std::string> KnownAir::load(const sd::Collections& data) {
    S3G_TRY(auto loaded, settings::load(data));
    KnownAir out;
    out.settings = loaded.first;
    out.modes = loaded.second;
    S3G_TRY(out.wipeout.air_falling_min_up_y_260,
            data.float_("physics_wipeout", "default", "Wipeout_AirFallingMinUpY"));
    S3G_TRY(out.wipeout.air_falling_max_angle_264,
            data.float_("physics_wipeout", "default", "Wipeout_AirFallingMaxAngle"));
    //82D8DA84 binds physics_reckoning handle1600, layout pointer1604.
    S3G_TRY(auto words, data.words<4>("physics_reckoning", "default", "FlipAxisAdjustment"));
    out.flip_axis_adjustment = rs::from_bits(words);
    KnownAirState& s = out.state;
    s.landing_normal_64 = {0.0f, 1.0f, 0.0f, 0.0f};
    s.landing_heading_80 = {0.0f, 0.0f, 0.0f, 0.0f};
    s.trajectory_apex_96 = {0.0f, 0.0f, 0.0f, 0.0f};
    s.collision_position_112 = {0.0f, 0.0f, 0.0f, 0.0f};
    s.selector_vector_128 = {0.0f, 0.0f, 0.0f, 0.0f};
    s.trajectory_follow_offset_144 = {0.0f, 0.0f, 0.0f, 0.0f};
    s.target_com_position_160 = {0.0f, 0.0f, 0.0f, 0.0f};
    s.collision_normal_speed_176 = 0.0f;
    s.time_in_state_180 = 0.0f;
    s.start_y_184 = 0.0f;
    s.max_y_188 = 0.0f;
    s.com_max_y_192 = 0.0f;
    s.collision_time_196 = 0.0f;
    s.time_to_apex_200 = 0.0f;
    s.body_flip_target_speed_204 = 0.0f;
    s.reached_apex_208 = false;
    s.landing_heading_valid_209 = false;
    s.start_flipped_210 = false;
    s.body_flipping_211 = false;
    s.grind_air_adjust_activated_212 = false;
    s.targeting_grind_213 = false;
    s.trajectory_index_216 = 0;
    return out;
}

rs::Result<KnownAirModeSettings, std::string> KnownAir::mode(std::uint32_t index) const {
    if (std::size_t(index) < modes.size()) return modes[index];
    return rs::make_err(fmt::cat("Undefined KnownAir physics mode ", index));
}

rs::Result<KnownAirFrame, std::string> frame(const SkaterRuntime& skater, std::uint32_t next) {
    const auto& p = skater.player_input.processed;
    if (!skater.player_input.toolkit) return rs::make_err(std::string("KnownAir requires current BoardToolkit"));
    const auto& t = *skater.player_input.toolkit;
    const auto* s = skater.trajectory.selector.selection();
    if (s == nullptr) return rs::make_err(std::string("KnownAir requires a completed selected trajectory"));
    KnownAirFrame f;
    f.start_flip_reference_96 = t.deck[2];
    f.alternate_head_target_112 = t.deck[3];
    f.velocity_400 = rs::from_bits(p.vectors_400_416[0]);
    f.ground_normal_464 = rs::from_bits(p.vectors_464_480_496_512_528[0]);
    f.start_height_484 = rs::from_bits(p.vectors_464_480_496_512_528[1][1]);
    f.skater_up_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
    f.flags_2468 = p.flags_2468;
    f.flags_2472 = p.flags_2472;
    f.flags_2476 = p.flags_2476;
    f.flags_2480 = p.flags_2480;
    f.flags_2484 = p.flags_2484;
    f.flags_2488 = p.flags_2488;
    f.next_physics_state_2500 = std::int32_t(next);
    f.delta_time_2604 = p.timestep_2604;
    f.forward_speed_2612 = p.scalar_2612;
    f.body_spin_input_2640 = skater.animation_input.fields.body_spin;
    f.selector_landing_normal_2656 = s->landing_normal;
    return f;
}

KnownAirReckoningFields reckoning(const GamePhysics& physics, const SkaterRuntime& skater) {
    const auto up = physics.riding.reckoning.up;
    KnownAirReckoningFields r;
    r.landing_normal_1152 = {up.x, up.y, up.z, 0.0f};
    r.heading_axis_1200 = physics.riding.reckoning_frames.heading;
    r.body_spin_angle_1568 = skater.air_reckoning.state.spin_angle;
    r.body_spin_speed_1572 = skater.air_reckoning.state.spin_speed;
    return r;
}

Status enter(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_TRY(const auto f, frame(skater, 201));
    auto state = skater.known_air.state;
    const auto settings = skater.known_air.settings;
    S3G_TRY(const auto mode, skater.known_air.mode(skater.player_input.processed.state_variant_index_2528));
    S3G_TRY(auto r, runtime::Runtime::new_(physics, skater));
    known::enter(state, f, settings, mode, r);
    S3G_CHECK(r.finish());
    skater.known_air.state = state;
    return ok();
}

Status update(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_TRY(const auto f, frame(skater, 201));
    auto state = skater.known_air.state;
    const auto settings = skater.known_air.settings;
    S3G_TRY(const auto mode, skater.known_air.mode(skater.player_input.processed.state_variant_index_2528));
    const auto rk = reckoning(physics, skater);
    S3G_TRY(auto r, runtime::Runtime::new_(physics, skater));
    known::update(state, f, settings, mode, rk, r);
    S3G_CHECK(r.finish());
    skater.known_air.state = state;
    return ok();
}

Status exit(GamePhysics& physics, SkaterRuntime& skater, std::uint32_t next) {
    S3G_TRY(auto f, frame(skater, next));
    auto state = skater.known_air.state;
    const auto settings = skater.known_air.settings;
    auto rk = reckoning(physics, skater);
    S3G_TRY(auto r, runtime::Runtime::new_(physics, skater));
    known::exit(state, f, settings, rk, r);
    S3G_CHECK(r.finish());
    skater.player_input.processed.scalar_2612 = f.forward_speed_2612;
    skater.air_reckoning.state.spin_angle = rk.body_spin_angle_1568;
    skater.air_reckoning.state.spin_speed = rk.body_spin_speed_1572;
    skater.known_air.state = state;
    return ok();
}

///82D36590: apex observation, shared air-wipeout(true), then upside-down request.
Status post_physics(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_TRY(const auto f, frame(skater, 201));
    auto state = skater.known_air.state;
    const auto settings = skater.known_air.wipeout;
    S3G_TRY(const auto mode, skater.known_air.mode(skater.player_input.processed.state_variant_index_2528));
    KnownAirWipeoutRequest request{};
    request.requested_35 = false;
    request.scalar_116 = 0.0f;
    request.counter_200 = 0;
    S3G_TRY(auto r, runtime::Runtime::new_(physics, skater));
    known::update_post_physics(state, f, mode, settings, request, r);
    S3G_CHECK(r.finish());
    if (request.counter_200 != 0) {
        skater.wipeout.state.request(15, request.scalar_116);
    }
    skater.known_air.state = state;
    return ok();
}

rs::Result<KnownAirOutput, std::string> fill(GamePhysics& physics, SkaterRuntime& skater) {
    S3G_TRY(const auto f, frame(skater, 201));
    const auto state = skater.known_air.state;
    auto out = output::storage(skater.player_input.physical.air);
    S3G_TRY(auto r, runtime::Runtime::new_(physics, skater));
    known::fill_physics_output(state, f, out, r);
    S3G_CHECK(r.finish());
    output::publish(out, skater.player_input.physical.air);
    return out;
}

}  // namespace s3::game::physics::known_air
