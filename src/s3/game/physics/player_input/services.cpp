// Port of skate-game/src/physics/player_input/services.rs.
//! Host ownership for the original 82DB4048 input phase. Every numerical
//! preparation is performed here; the two callbacks address the shared
//! skeleton/whole-player owners that cannot be borrowed twice by the host.
#include "s3/game/physics/player_input/services.hpp"

#include "s3/game/physics/ground_runtime.hpp"
#include "s3/game/physics/player_input/ground_position.hpp"
#include "s3/game/physics/player_input/reset.hpp"

namespace s3::game::physics::player_input::services {

using E = std::string;

Services::R<rs::Unit> Services::update_pre_input_manager_82d81610(PlayerInputState& player,
                                                                  PhysicalPlayerInput& /*physical*/) {
    return pre_input.prepare(player.manager_1856_counter_320);
}

Services::R<rs::Unit> Services::reset_processed_input_82bf9ef0(ProcessedPhysicsInput& output) {
    reset::reset_processed(output);
    return rs::Unit{};
}

Services::R<std::uint32_t> Services::actor_query_slot_56() { return host.actor_query_56; }

Services::R<std::uint32_t> Services::actor_query_slot_44() { return host.actor_query_44; }

Services::R<rs::Unit> Services::reset_player_probe_82d7a330(PlayerInputState& player,
                                                            PhysicalPlayerInput& /*physical*/) {
    // Native clears every represented field after copying it to Processed.
    player.probe = {};
    return rs::Unit{};
}

Services::R<rs::Unit> Services::check_teleport_82db88c8(PlayerInputState& player, PhysicalPlayerInput& physical,
                                                        ProcessedPhysicsInput& output) {
    // Native handles its state request before its explicit reset bit.
    // Host requests remain owned until the entire reset succeeds.
    if (physical.state.flag_61 != 0) {
        const AnimationPartTransform target = host.published_board_transform;
        S3G_CHECK(callbacks.teleport(board, ground, target, player, physical, output));
    }
    if ((player.flags_1296 & (1u << 19)) != 0) {
        const auto deck = board.part_transforms()[6];
        AnimationPartTransform target{};
        for (std::size_t c = 0; c < 3; ++c) {
            for (std::size_t i = 0; i < 3; ++i) target[c][i] = deck.basis.columns[c][i];
        }
        const auto p = deck.translation;
        target[3] = {p.x, p.y, p.z, 0.0f};
        S3G_CHECK(callbacks.teleport(board, ground, target, player, physical, output));
        player.flags_1296 &= ~(1u << 19);
    }
    if (pending_teleport.has_value()) {
        const AnimationPartTransform target = *pending_teleport;
        S3G_CHECK(callbacks.teleport(board, ground, target, player, physical, output));
        pending_teleport = std::nullopt;
    }
    return rs::Unit{};
}

Services::R<bool> Services::actor_input_available_slot_4() { return host.input_available; }

Services::R<float> Services::transition_action_825903c8() { return host.transition_action; }

Services::R<RawVector> Services::calculate_ground_position_82c02840(const PhysicalPlayerInput& /*physical*/) {
    return ground_position::ground_position(board, ground_frame);
}

Services::R<rs::Unit> Services::prepare_board_toolkit_82c013f0(PlayerInputState& /*player*/,
                                                               PhysicalPlayerInput& /*physical*/,
                                                               ProcessedPhysicsInput& output) {
    BoardToolkit t = ground.prepare_toolkit(board, output);
    // 82C01744 publishes this during toolkit preparation, not PhysOut fill.
    output.scalar_2616 = t.absolute_speed;
    toolkit = std::move(t);
    return rs::Unit{};
}

Services::R<rs::Unit> Services::process_skeleton_82bd8918(const AnimationInputPacket& packet,
                                                          PhysicalPlayerInput& physical,
                                                          ProcessedPhysicsInput& output) {
    if (!toolkit.has_value()) {
        return rs::make_err(std::string("Input toolkit must precede Skeleton::ProcessData"));
    }
    S3G_CHECK(callbacks.process_skeleton(board, *toolkit, packet, physical, output));
    return rs::Unit{};
}

Services::R<rs::Unit> Services::update_grind_manager_82d8a828(PlayerInputState& /*player*/,
                                                              PhysicalPlayerInput& /*physical*/,
                                                              ProcessedPhysicsInput& output) {
    if (pending_grind.has_value()) {
        return rs::make_err(std::string("Previous grind input was not consumed by PostInput"));
    }
    S3G_TRY(auto pending, callbacks.prepare_grind(board, grind, output, world, world_edges, host.air_counter_40));
    pending_grind = std::move(pending);
    return rs::Unit{};
}

}  // namespace s3::game::physics::player_input::services
