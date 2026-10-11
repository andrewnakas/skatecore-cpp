// Port of skate-core/src/player/input_phase/publication.rs (whole file).
#include "s3/core/player/input_phase/publication.hpp"

namespace s3::player::input_phase::publication {

using types::NativeReferenceBase;
using types::NativeRelativeReference;

void publish_player_flags(PlayerInputState& player, ProcessedPhysicsInput& out) {
    transfer_bit(out.flags_2472, 14, player.flags_1296, 27);
    transfer_bit(out.flags_2488, 26, player.flags_1296, 20);
    out.vectors_880_896_912_928_944[3] = player.queued_vector_1280;
    transfer_bit(out.flags_2488, 22, player.flags_1296, 18);
    transfer_bit(out.flags_2480, 16, player.flags_1296, 17);
    player.queued_vector_1280 = {0, 0, 0, 0};
    player.flags_1296 &= ~(1u << 20);
}

void publish_force_braking(const PlayerInputState& player, const PhysicalPlayerInput& physical,
                           const AnimationInputPacket& packet, ProcessedPhysicsInput& out) {
    const bool force = packet.force_braking_10796 != 0 && static_cast<std::int32_t>(player.state_count_1312) > 5 &&
                       physical.skateboard.scalar_160 < rs::from_bits(0x3dcccccdu) &&
                       physical.state.state_16 == 100 &&
                       static_cast<std::int32_t>(physical.collision.wheel_count_0) > 2;
    replace_bool(out.flags_2476, 24, force);
}

void publish_collision_prefix(const PlayerInputState& player, const PhysicalPlayerInput& physical,
                              ProcessedPhysicsInput& out) {
    replace_byte(out.flags_2484, 26, physical.collision.flag_215);
    replace_byte(out.flags_2484, 22, physical.collision.flag_216);
    out.vectors_720_784_800_816_832_864[5] = player.manager_1852_vector_176;
}

void publish_animation_packet(const AnimationInputPacket& packet, ProcessedPhysicsInput& out) {
    input::animation_packet::ProcessedPacketFields fields{};
    fields.flags_2468 = out.flags_2468;
    fields.flags_2476 = out.flags_2476;
    fields.timestep = out.timestep_2604;
    fields.scalar_2668 = out.scalar_2668;
    fields.vector_1520 = out.vector_1520;
    fields.matrix_1536 = out.matrix_1536;
    fields.byte_1600 = out.byte_1600;
    fields.truck_tightness = out.truck_tightness_2760;
    fields.scalar_2764 = out.scalar_2764;
    input::animation_packet::publish(packet.publication, fields);
    out.flags_2468 = fields.flags_2468;
    out.flags_2476 = fields.flags_2476;
    out.timestep_2604 = fields.timestep;
    out.scalar_2668 = fields.scalar_2668;
    out.vector_1520 = fields.vector_1520;
    out.matrix_1536 = fields.matrix_1536;
    out.byte_1600 = fields.byte_1600;
    out.truck_tightness_2760 = fields.truck_tightness;
    out.scalar_2764 = fields.scalar_2764;
}

namespace {
void select_surface_mode(std::uint32_t requested, ProcessedPhysicsInput& out) {
    std::uint32_t mode = 1;
    if (requested >= 1 && requested <= 5) {
        mode = requested;
    } else if (requested == 6) {
        mode = 3;
    }
    std::uint16_t primary = 0;
    std::uint16_t secondary = 0;
    switch (mode) {
        case 1: primary = 24; secondary = 104; break;
        case 2: primary = 40; secondary = 116; break;
        case 3: primary = 56; secondary = 128; break;
        case 4: primary = 72; secondary = 140; break;
        case 5: primary = 88; secondary = 152; break;
        default: S3_PANIC("surface mode normalization is exhaustive");
    }
    out.surface_mode_2540 = mode;
    out.surface_primary_ref_2544 = NativeRelativeReference{NativeReferenceBase::SurfaceSelector, primary};
    out.surface_secondary_ref_2552 = NativeRelativeReference{NativeReferenceBase::SurfaceSelector, secondary};
}

void publish_wheel_contacts(const PhysicalPlayerInput& physical, ProcessedPhysicsInput& out) {
    const auto& wheel = physical.collision.wheel_contact_3296_3299;
    const bool outer_tail = wheel[2] != 0 || wheel[3] != 0;
    const bool outer_nose = wheel[0] != 0 || wheel[1] != 0;
    const bool left_pair = wheel[2] != 0 || wheel[0] != 0;
    const bool right_pair = wheel[3] != 0 || wheel[1] != 0;
    const bool switched = (out.flags_2468 & (1u << 20)) != 0;
    replace_bool(out.flags_2472, 27, switched ? outer_nose : outer_tail);
    replace_bool(out.flags_2472, 26, switched ? outer_tail : outer_nose);
    replace_bool(out.flags_2472, 25, switched ? right_pair : left_pair);
    replace_bool(out.flags_2472, 24, switched ? left_pair : right_pair);
}
}  // namespace

rs::Result<rs::Unit, std::uint32_t> select_surface(const PlayerInputState& player, const AnimationInputPacket& packet,
                                                   std::uint32_t surface_default_mode, ProcessedPhysicsInput& out) {
    const std::uint32_t index = packet.state_variant_10928;
    if (index >= player.state_variants_1408.size()) {
        out.state_variant_index_2528 = index;
        return rs::make_err(index);
    }
    const auto& variant = player.state_variants_1408[index];
    out.state_variant_ref_2548 = NativeRelativeReference{
        NativeReferenceBase::Player, static_cast<std::uint16_t>(0x580u + 0x10u * static_cast<std::uint16_t>(index))};
    out.state_variant_index_2528 = index;

    std::uint32_t requested = surface_default_mode;
    if (variant.surface_override_enabled_60 != 0) {
        if (surface_default_mode >= 1 && surface_default_mode <= 3) {
            requested = 1;
        } else if (surface_default_mode == 5) {
            requested = 3;
        } else if (surface_default_mode == 13) {
            requested = 2;
        }
    }
    select_surface_mode(requested, out);
    return rs::Unit{};
}

void publish_external_physics(PlayerInputState& player, const AnimationInputPacket& packet,
                              ProcessedPhysicsInput& out) {
    if (packet.use_external_physics_10688 != 0) {
        out.flags_2472 |= 1u << 29;
        out.external_physics_1616.copy_from(packet.external_physics_10512);
        if ((out.external_physics_1616.flags & (1u << 26)) != 0) {
            player.external_physics_cache_1008.copy_from(packet.external_physics_10512);
        } else {
            for (std::size_t i = 5; i < 9; ++i) {
                out.external_physics_1616.vectors[i] = player.external_physics_cache_1008.vectors[i];
            }
            out.external_physics_1616.vectors[9] = player.external_physics_cache_1008.vectors[9];
            transfer_bit(out.external_physics_1616.flags, 27, player.external_physics_cache_1008.flags, 27);
            out.external_physics_1616.flags |= 1u << 26;
        }
    }
    replace_byte(out.flags_2472, 28, packet.external_physics_flag_10689);
}

void publish_state_prefix(PlayerInputState& player, const PhysicalPlayerInput& physical,
                          const AnimationInputPacket& packet, std::uint32_t captured_state,
                          ProcessedPhysicsInput& out) {
    transfer_bit(out.flags_2468, 13, player.flags_1296, 31);
    const bool has_probe_or_animation_flag = (out.flags_2468 & (1u << 2)) != 0 || out.probe_1792.byte_104 != 0;
    replace_bool(out.flags_2468, 18, has_probe_or_animation_flag);
    player.frames_since_teleport_1328 =
        (player.flags_1296 & (1u << 29)) != 0 ? 0u : player.frames_since_teleport_1328 + 1u;
    out.frames_since_teleport_2584 = player.frames_since_teleport_1328;
    player.flags_1296 &= ~(1u << 29);
    out.state_count_2564 = player.state_count_1312;
    out.state_timer_2664 = player.state_timer_1344;
    out.update_count_2568 = player.update_count_1316;
    replace_byte(out.flags_2472, 12, packet.flag_10373);
    replace_byte(out.flags_2468, 0, packet.flag_10786);
    replace_byte(out.flags_2472, 31, packet.flag_10787);
    out.state_2508 = captured_state;
    out.state_2504 = captured_state;
    out.category_2512 = physical.state.category_12;
    replace_byte(out.flags_2472, 30, physical.state.flag_61);
    replace_byte(out.flags_2472, 18, physical.state.flag_69);
    replace_byte(out.flags_2480, 31, physical.state.flag_74);
    out.grind_words_2532_2536 = physical.grinds.words_136_140;
    out.category_2516 = physical.state.category_12;
    out.player_state_value_2520 = player.state_value_1336;
}

void publish_physical_outputs(const PhysicalPlayerInput& physical, const AnimationInputPacket& packet,
                              ProcessedPhysicsInput& out) {
    out.vectors_400_416[0] = physical.skateboard.vector_80;
    out.vectors_400_416[1] = physical.skateboard.vector_96;
    out.scalar_2736 = physical.skateboard.scalar_172;
    out.scalar_2612 = physical.skateboard.scalar_168;
    out.scalar_2656 = physical.skateboard.scalar_164;
    out.scalar_2652 = physical.skateboard.scalar_160;
    out.vectors_720_784_800_816_832_864[0] = physical.skateboard.vector_64;
    out.vectors_720_784_800_816_832_864[1] = physical.skateboard.vector_128;
    out.vectors_720_784_800_816_832_864[2] = physical.skateboard.vector_144;

    out.vectors_624_640_656_672[0] = packet.vector_10816;
    out.vectors_624_640_656_672[1] = packet.vector_10832;
    transfer_bit(out.flags_2476, 6, packet.flags_10932, 31);
    transfer_bit(out.flags_2476, 5, packet.flags_10932, 30);
    transfer_bit(out.flags_2476, 4, packet.flags_10932, 29);
    out.scalar_2696 = packet.scalar_10912;
    out.vectors_624_640_656_672[2] = packet.vector_10848;
    out.scalar_2700 = packet.scalar_10916;
    out.vectors_624_640_656_672[3] = packet.vector_10864;
    replace_byte(out.flags_2480, 0, packet.flag_10369);
    out.vectors_880_896_912_928_944[0] = packet.vector_10880;
    out.vectors_880_896_912_928_944[1] = packet.vector_10896;
    transfer_bit(out.flags_2488, 24, packet.flags_10932, 22);

    out.vectors_544_560_592_608[2] = physical.reckoning.vector_64;
    out.vectors_880_896_912_928_944[4] = physical.reckoning.vector_144;
    replace_byte(out.flags_2488, 21, physical.reckoning.flag_164);
    out.vectors_544_560_592_608[3] =
        physical.air.use_air_reckoning_452 != 0 ? physical.air.vector_160 : physical.reckoning.vector_16;
    out.vectors_544_560_592_608[0] = physical.reckoning.vector_96;
    out.vectors_544_560_592_608[1] = physical.board_reckoning_side_176;
    out.vectors_464_480_496_512_528[0] = physical.ground.vector_96;
    out.vectors_464_480_496_512_528[4] = physical.ground.vector_64;
    out.vectors_720_784_800_816_832_864[4] = physical.ground.vector_128;
    replace_byte(out.flags_2476, 27, physical.ground.flag_317);
    replace_byte(out.flags_2476, 23, physical.ground.flag_318);

    out.air_scalar_2772 = physical.air.scalar_184;
    replace_byte(out.flags_2476, 26, physical.air.flag_441);
    replace_byte(out.flags_2476, 0, physical.air.flag_446);
    replace_byte(out.flags_2480, 25, physical.air.flag_447);
    replace_byte(out.flags_2480, 24, physical.air.flag_448);
    transfer_bit(out.flags_2480, 30, physical.component_1832_word_1876, 28);
    replace_byte(out.flags_2468, 9, physical.grinds.flag_318);
    replace_byte(out.flags_2468, 8, physical.grinds.flag_322);

    replace_byte(out.flags_2468, 15, physical.collision.flag_3478);
    replace_byte(out.flags_2468, 16, physical.collision.flag_3475);
    replace_byte(out.flags_2468, 17, physical.collision.flag_3472);
    out.wheel_count_2556 = physical.collision.wheel_count_0;
    replace_byte(out.flags_2468, 14, physical.collision.flag_3477);
    out.collision_scalar_2924 = physical.collision.scalar_28;
    replace_byte(out.flags_2488, 30, physical.collision.flag_3481);
    replace_byte(out.flags_2472, 11, physical.physics.flag_32);
    out.vectors_720_784_800_816_832_864[3] = physical.physics.vector_16;
    replace_byte(out.flags_2472, 9, physical.skeleton.flag_597);
    replace_byte(out.flags_2480, 10, physical.skeleton.flag_600);
    replace_byte(out.flags_2480, 9, physical.skeleton.flag_601);
    out.filtered_state_2524 = physical.filtered_state_0;

    replace_byte(out.flags_2476, 21, physical.off_board.flag_304);
    replace_bool(out.flags_2476, 17, physical.off_board.scalar_32 <= 0.0f);
    replace_byte(out.flags_2480, 15, physical.off_board.flag_311);
    replace_byte(out.flags_2476, 16, physical.off_board.flag_315);
    replace_byte(out.flags_2476, 8, physical.off_board.flag_316);
    out.off_board_scalar_2832 = physical.off_board.scalar_32;
    replace_byte(out.flags_2480, 1, physical.off_board.flag_318);
    replace_byte(out.flags_2484, 23, physical.off_board.flag_314);
    out.vectors_880_896_912_928_944[2] = physical.off_board.vector_64;
    replace_byte(out.flags_2484, 16, physical.off_board.flag_328);
    publish_wheel_contacts(physical, out);
}

RawMatrix effective_animation_transform(RawMatrix source, std::uint32_t flags_2476) {
    RawMatrix result = source;
    if ((flags_2476 & (1u << 2)) != 0) {
        for (std::size_t row : {std::size_t(0), std::size_t(2)}) {
            for (auto& lane : result[row]) {
                lane = rs::to_bits(-rs::from_bits(lane));
            }
        }
    }
    return result;
}

void publish_line_tests(const PlayerInputState& player, ProcessedPhysicsInput& out) {
    out.line_tests_960_1008_1056 = {player.left_line_test_1536, player.right_line_test_1584,
                                    player.hips_line_test_1488};
    if (out.line_tests_960_1008_1056[0].valid != 0) {
        out.left_surface_2596 = out.line_tests_960_1008_1056[0].surface;
    }
    if (out.line_tests_960_1008_1056[1].valid != 0) {
        out.right_surface_2600 = out.line_tests_960_1008_1056[1].surface;
    }
}

}  // namespace s3::player::input_phase::publication
