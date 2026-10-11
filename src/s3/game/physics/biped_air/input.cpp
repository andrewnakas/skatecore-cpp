// Port of skate-game/src/physics/biped_air/input.rs
//! Direct processed/physical inputs for the distinct BipedAir lifecycle.
#include "s3/game/physics/biped_air/input.hpp"

#include "s3/core/physics/skeleton_animation_record.hpp"
#include "s3/game/physics.hpp"
#include "s3/game/physics/skater.hpp"

namespace s3::game::physics::biped_air::input {

using ::s3::physics::skeleton_animation_record::compose_affine;

Context context(const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    Context c{};
    c.selection_flags_2948 = p.actor_query_2948;
    c.matching_group_2952 = static_cast<std::int32_t>(p.actor_query_2952);
    c.up_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
    c.forward_224 = rs::from_bits(p.effective_anim_transform_192[2]);
    return c;
}

Frame effective_frame(const SkaterRuntime& skater) {
    //82BE3650: actual animation root, with effective stance axes0/2.
    auto frame = skater.animated_skeleton.roots.animation_to_world;
    if ((skater.player_input.processed.flags_2476 & 4u) != 0) {
        for (std::size_t axis : {std::size_t(0), std::size_t(2)}) {
            for (float& v : frame[axis]) v = -v;
        }
    }
    return frame;
}

EnterInput enter(const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    EnterInput e{};
    e.animation_frame = effective_frame(skater);
    e.body_position_15872 = skater.animated_skeleton.board_frames.com_frame[3];
    e.body_position_15936 = skater.animated_skeleton.board_frames.lifted_com_frame[3];
    e.position_592 = rs::from_bits(p.vectors_544_560_592_608[2]);
    e.up_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
    e.flags_2484 = p.flags_2484;
    return e;
}

HeightInput height(const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    const auto root = skater.animated_skeleton.roots.animation_to_world;
    HeightInput h{};
    h.bone15 = compose_affine(root, skater.animated_skeleton.record.pose[15])[3];
    h.bone19 = compose_affine(root, skater.animated_skeleton.record.pose[19])[3];
    //82BE3170 reads mapped physical target12624; NOT record6480.
    h.bone1 = compose_affine(root, skater.skeleton_input.drive_frames[1])[3];
    h.up_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
    h.velocity_608 = rs::from_bits(p.vectors_544_560_592_608[3]);
    return h;
}

rs::Result<Packet, std::string> launch_packet(const SkaterRuntime& skater) {
    const auto& p = skater.player_input.processed;
    S3G_TRY_OPT(const auto* board, skater.player_input.toolkit ? &*skater.player_input.toolkit : nullptr,
                "BipedAir launch requires the completed BoardToolkit");
    Processed input{};
    input.board_position_112 = board->deck[3];
    input.forward_224 = rs::from_bits(p.effective_anim_transform_192[2]);
    input.up_544 = rs::from_bits(p.vectors_544_560_592_608[0]);
    input.position_592 = rs::from_bits(p.vectors_544_560_592_608[2]);
    input.velocity_608 = rs::from_bits(p.vectors_544_560_592_608[3]);
    input.velocity_912 = rs::from_bits(p.vectors_880_896_912_928_944[2]);
    input.departure_geometry =
        DepartureGeometry{rs::from_bits(p.grind.point_1120), rs::from_bits(p.grind.direction_1136)};
    input.flags_2472 = p.flags_2472;
    input.flags_2476 = p.flags_2476;
    input.flags_2480 = p.flags_2480;
    input.previous_state_2504 = p.state_2504;
    input.current_state_2508 = p.state_2508;
    input.current_category_2512 = p.category_2512;
    input.previous_category_2516 = p.category_2516;
    input.raw_x_2692 = skater.animation_input.extra.biped_world_x;
    input.raw_z_2688 = skater.animation_input.extra.biped_world_z;
    const auto [turn, settings] = skater.biped_ground.air_settings();
    auto r = skater.biped_air.state.launch_packet(input, skater.biped_ground.controller.state, turn, settings);
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return r.unwrap();
}

Status submit(const GamePhysics& physics, SkaterRuntime& skater, Packet packet) {
    const auto c = context(skater);
    const auto g = physics.settings.step.simulation.gravity_acceleration;
    auto r = skater.offboard_air_selector.launch(physics.world, packet, {g.x, g.y, g.z, 0.0f}, c);
    if (r.is_err()) return rs::make_err(std::string(r.unwrap_err()));
    return ok();
}

}  // namespace s3::game::physics::biped_air::input
