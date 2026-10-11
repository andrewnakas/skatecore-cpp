// Port of skate-game/src/physics/air_phase/input.rs.
//! Actual Processed/animation/stock owners read by PhysicsAir and its launch.
#include "s3/game/physics/air_phase/input.hpp"

#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::air_phase::input {

rs::Result<AirSettings, std::string> AirSettings::load(const sd::Collections& data) {
    S3G_TRY(const auto words, data.words<16>("physics_airstates", "default", "BodySpinInputFilter"));
    const auto values = rs::from_bits(words);
    auto f = [&](const char* name) { return data.float_("physics_airstates", "default", name); };
    std::array<float, 5> distances{0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::size_t i = 0;
    for (const char* mode : {"easy", "normal", "hardcore", "motorized", "test"}) {
        S3G_TRY(distances[i], data.float_("physics_mode", mode, "GrindLockDist"));
        ++i;
    }
    AirSettings out;
    for (std::size_t k = 0; k < 8; ++k) {
        out.state.body_spin_over_time_320.x[k] = values[k];
        out.state.body_spin_over_time_320.y[k] = values[8 + k];
    }
    S3G_TRY(out.state.landing_normal_blend_388, f("SpeedToAlignToGround_PhysAir"));
    S3G_TRY(out.state.body_spin_scale_428, f("MaxSpinSpeed"));
    S3G_TRY(out.state.landing_normal_angle_limit_444, f("DontAlignAnglePhysicsAir"));
    S3G_TRY(out.steering_blend, data.float_("physics_steering", "default", "SteeringTiltBlending"));
    out.grind_lock_distance = distances;
    return out;
}

PhysicsAirFrame frame(const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    PhysicsAirFrame f;
    f.current_velocity_400 = rs::from_bits(p.vectors_400_416[0]);
    f.ground_position_y_500 = rs::from_bits(p.vectors_464_480_496_512_528[2][1]);
    f.trajectory_position_592 = rs::from_bits(p.vectors_544_560_592_608[2]);
    f.trajectory_velocity_608 = rs::from_bits(p.vectors_544_560_592_608[3]);
    f.jump_velocity_848 = rs::from_bits(skater.player_state.post.jump_reference);
    f.flags_2468 = p.flags_2468;
    f.previous_physics_state_2504 = static_cast<std::int32_t>(p.state_2504);
    f.previous_physics_category_2516 = static_cast<std::int32_t>(p.category_2516);
    f.frames_since_jump_correction_2576 = static_cast<std::int32_t>(skater.player_state.post.jump_fix_frames);
    f.delta_time_2604 = p.timestep_2604;
    f.body_spin_input_2640 = skater.animation_input.fields.body_spin;
    f.gravity_y_2648 = p.gravity_2648;
    f.state_timer_2664 = p.state_timer_2664;
    return f;
}

rs::Result<SelectorInput, std::string> selector_input(const GamePhysics& physics, const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    const auto g = physics.settings.step.simulation.gravity_acceleration;
    SelectorInput s;
    s.gravity = {g.x, g.y, g.z, 0.0f};
    s.ground_normal = rs::from_bits(p.vectors_464_480_496_512_528[0]);
    s.contact_position = rs::from_bits(p.vectors_464_480_496_512_528[2]);
    s.heading_direction = rs::from_bits(p.vectors_464_480_496_512_528[3]);
    s.reference_up = rs::from_bits(p.vectors_544_560_592_608[0]);
    s.board_vertical_velocity = rs::from_bits(p.vectors_400_416[0][1]);
    s.directional_input = p.transition_2636;
    s.previous_physics_state = p.state_2504;
    s.flags_2472 = p.flags_2472;
    s.flags_2476 = p.flags_2476;
    s.offboard_flags_1776 = p.external_physics_1616.flags;
    if (std::size_t(p.state_variant_index_2528) >= skater.air_settings.grind_lock_distance.size()) {
        return rs::make_err(fmt::cat("Invalid trajectory physics mode ", p.state_variant_index_2528));
    }
    s.grind_lock_distance = skater.air_settings.grind_lock_distance[p.state_variant_index_2528];
    return s;
}

rs::Result<LaunchInfo, std::string> launch_info(const GamePhysics& physics, const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    if (!skater.player_input.toolkit) {
        return rs::make_err(std::string("Air launch requires current BoardToolkit"));
    }
    const auto& toolkit = *skater.player_input.toolkit;
    LaunchInfo info{};
    info.reckoning_transform = physics.riding.reckoning_frames.system;
    info.reckoning_inverse = physics.riding.reckoning_frames.inverse_system;
    info.start_velocity = rs::from_bits(p.vectors_400_416[0]);
    info.com_velocity = rs::from_bits(p.prepared_jump_704);
    info.skeleton_vector_160 = skater.animated_skeleton.board_frames.local_centre_of_mass;
    info.skeleton_vector_176 = skater.animated_skeleton.board_frames.local_board_position;
    info.board_position = toolkit.deck[3];
    info.animation_com_position = rs::from_bits(p.vectors_544_560_592_608[2]);
    info.cone_angle_x = skater.trajectory.settings.cone_x;
    info.cone_angle_z = skater.trajectory.settings.cone_z;
    info.timestep = p.timestep_2604;
    info.trajectory_count = (p.flags_2468 & 0x2000u) != 0 ? 7 : 1;
    return info;
}

}  // namespace s3::game::physics::air_phase::input
