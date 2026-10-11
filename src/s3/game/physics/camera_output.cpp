// Port of skate-game/src/physics/camera_output.rs.
//! Physical-output publications consumed by gameplay camera82DF69C0.
//! Reset82DE53F0 runs before the actual producers82DB6EC0/82D3A388.
#include "s3/game/physics/camera_output.hpp"

#include "s3/core/camera/path_evaluator.hpp"
#include "s3/game/camera.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"
#include "s3/game/state_names.hpp"

namespace s3::game::physics::camera_output {

namespace {

struct StaticWorld final : sc::camera::MovingObstacleProvider {
    std::size_t collect(sc::camera::F4, sc::camera::F4, float, std::array<sc::camera::PathObstacle, 50>&) override {
        // The subject actor is excluded by the native provider. Our current
        // world contains this subject and static triangles, with no other actors.
        return 0;
    }
};

}  // namespace

Status advance(const GamePhysics& physics, const SkaterRuntime& skater,
               const sc::animation::physical_feedback::PhysicalFeedback& feedback, camera::CameraRuntime& camera) {
    S3G_TRY_OPT(const auto* output, physics.exchange.output(),
                "Camera requires the completed physical output snapshot");
    if (physics.ticks == 0)
        return rs::make_err(std::string("Camera received physical output before the first completed tick"));
    const std::uint64_t completed_tick = physics.ticks - 1;
    if (output->tick != completed_tick || output->state != skater.player_state.current()) {
        return rs::make_err(fmt::cat("Camera received stale physical output: output_tick=", output->tick,
                                     ", completed_tick=", completed_tick, ", output_state=", debug_name(output->state),
                                     ", selected_state=", debug_name(skater.player_state.current())));
    }
    camera::CameraPreferences preferences{};
    // User preferences are owned here: ordinary uninverted controls.
    preferences.invert_look = {false, false};
    // Native preferences constructor82DF6294/629C initializes both
    // fields to zero.82DF8E80 only updates its air-side bytes37..39.
    preferences.shake_variant = 0;
    preferences.value_32 = 0.0f;
    S3G_TRY(const auto inputs,
            publish(skater, *output, feedback, skater.centre_of_mass_output, preferences,
                    // SkaterAnim vtable8231E170+28=82B97140: full15180 bit30.
                    std::uint8_t(skater.animation.stance().second),
                    1,  // Stable host player identity replaces the original actor pointer.
                    output->tick));
    S3G_TRY(const auto snapshot, camera::publish_camera_subject(physics, skater, inputs));
    camera::CameraGraphEnvironment environment{};
    environment.camera_type = 1;
    environment.on_road = false;
    environment.ledge_left = false;
    environment.ledge_right = false;
    const auto gravity = physics.settings.step.simulation.gravity_acceleration;
    StaticWorld moving;
    S3G_CHECK(camera.advance(physics.settings.step.simulation.time_step, snapshot, physics.world,
                             sc::camera::F4{gravity.x, gravity.y, gravity.z, 0.0f}, environment, moving));
    return ok();
}

rs::Result<camera::CameraPublicationInputs, std::string> publish(
    const SkaterRuntime& skater, const sc::physics::phase::PhysicalOutputSnapshot& output,
    const sc::animation::physical_feedback::PhysicalFeedback& feedback,
    sc::physics::centre_of_mass_filter::CentreOfMassOutput com, camera::CameraPreferences preferences,
    std::uint8_t skater_animation_stance, std::uint32_t context, std::uint64_t tick) {
    const auto& p = skater.player_input.processed;
    const auto& physical = skater.player_input.physical;
    const auto& fields = skater.animation_input.fields;
    const std::uint32_t intents = skater.animation_input.output.flags;
    const auto& packet = skater.animation.packet;
    camera::CameraPublicationInputs out{};
    out.tick = tick;
    out.state.height_32 = physical.state.surface_height_32;
    out.state.physically_pushing_55 = bit(p.flags_2468, 25);
    out.state.wiping_out_59 = bit(p.flags_2468, 18);
    out.state.manual_60 = std::uint8_t(fields.balance != 0.0f);
    out.state.reset_62 = bit(p.flags_2472, 10);
    out.state.use_skeleton_root_75 = std::uint8_t(physical.state.category_12 == 500);
    out.state.flag_79 = std::uint8_t(p.state_variant_index_2528 == 3);
    out.state.flag_81 = std::uint8_t(skater.player_state.state_flags[81 - 52]);

    out.animation.conditioned_turn = feedback.conditioned_turn;
    out.animation.input_turn_64 = fields.turn;
    out.animation.input_kickturn_68 = p.spin_input_2672;
    out.animation.time_since_input_128 = p.time_since_last_input_2748;
    out.animation.wipeout_tweak_148 = physical.animation.profile_148;
    out.animation.stance_155 = std::uint8_t(packet.riding_fakie);
    out.animation.running_out_160 = bit(p.flags_2480, 2);
    out.animation.skater_animation_stance = skater_animation_stance;

    // Consume current output, including KnownAir82D36880's predicted
    // landing/apex. Other states retain their actual template fields.
    out.air.apex_0 = rs::from_bits(physical.air.trajectory_apex_0);
    out.air.landing_position_16 = rs::from_bits(physical.air.collision_position_16);
    out.air.landing_normal_32 = rs::from_bits(physical.air.landing_normal_32);
    out.air.launch_position_48 = rs::from_bits(physical.air.selector_vector_48);
    out.air.heading_80 = rs::from_bits(physical.air.landing_heading_80);
    out.air.time_176 = physical.air.time_in_state_176;
    out.air.duration_180 = physical.air.collision_time_180;
    out.air.apex_time_196 = physical.air.time_to_apex_196;
    out.air.flag_440 = bit(p.flags_2468, 22);

    out.offboard = offboard_output(physical.off_board);

    out.grinds.direction_0 = rs::from_bits(physical.grinds.direction_0);
    out.grinds.camera_target_96 = rs::from_bits(physical.grinds.camera_target_96);
    out.grinds.grinding_316 = physical.grinds.grinding_316;

    out.events.intent_51 = bit(intents, 28);
    out.events.preparing_52 = bit(intents, 27);
    out.events.dropping_in_63 = bit(intents, 22);
    out.events.trick_125 = bit(p.flags_2480, 11);
    // Completed State503 Fill82D4E0A8, consumed as Ground322 by the camera.
    out.events.hippy_jump_322 = physical.ground.hippy_jumping_322;
    // Scoring2 reset82DE4468; HoM duration stays inactive in this host.
    out.events.broken_bone_duration_200 = 0.0f;
    // AirCollector82DA78D0 publishes the world conditioner capability mask.
    out.events.capabilities_204 = physical.scoring.capabilities_204;

    out.damped_com_80 = com.position;
    // 82C03304..3340 copies actual BoardBody80 into Ground80 and96.
    out.ground_up_80 = {output.ground_normal.x, output.ground_normal.y, output.ground_normal.z, 0.0f};
    // Ground288 resets82DE3728; selected ordinary Ground Fill leaves it.
    out.ground_scalar_288 = 0.0f;
    out.look_552_556 = {skater.animation_input.extra.look_x, skater.animation_input.extra.look_y};
    out.collision_look_target_64 = {output.predicted_position.x, output.predicted_position.y,
                                    output.predicted_position.z, 0.0f};
    out.preferences = preferences;
    out.context = context;
    return out;
}

camera::CameraOffboardOutput offboard_output(const sc::player::input_phase::OffBoardOutputFields& output) {
    camera::CameraOffboardOutput out{};
    out.duration_92 = output.scalar_92;
    out.time_152 = output.scalar_152;
    out.apex_time_156 = output.scalar_156;
    // 82DF6B74..6B8C supplies OffBoard160 to setter52 at82DF6CA0.
    out.launch_normal_160 = rs::from_bits(output.vector_160);
    out.launch_position_176 = rs::from_bits(output.vector_176);
    out.landing_normal_192 = rs::from_bits(output.vector_192);
    out.landing_position_208 = rs::from_bits(output.vector_208);
    out.heading_224 = rs::from_bits(output.vector_224);
    out.apex_240 = rs::from_bits(output.vector_240);
    // 82DF74E4 reads308; the camera packet's historical name is misleading.
    out.object_held_304 = output.flag_308;
    out.hurdle_317 = output.hippy_hurdling_317;
    out.use_trajectory_331 = output.trajectory_valid_331;
    // 82DF7474..7490 combines this byte with the dropping-in event.
    out.dropping_in_334 = output.flag_334;
    return out;
}

}  // namespace s3::game::physics::camera_output
